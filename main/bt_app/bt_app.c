//
// Created by Kok on 12/1/24.
//


#include <esp_log.h>

#include "bt_app.h"

#include <string.h>
#include <host/ble_gatt.h>
#include <host/ble_hs_id.h>
#include <host/ble_hs.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>

static const char TAG[] = "bt_app";

static uint8_t ble_addr_type = 0;

static const uint8_t keyboard_report_map[] = {
    0x05, 0x01,         // Usage Page (Generic Desktop Ctrls)
    0x09, 0x06,         // Usage (Keyboard)
    0xA1, 0x01,         // Collection (Application)

    // Modifier keys (1 byte)
    0x05, 0x07,         //   Usage Page (Keyboard/Keypad)
    0x19, 0xE0,         //   Usage Minimum (Left Control)
    0x29, 0xE7,         //   Usage Maximum (Right GUI)
    0x15, 0x00,         //   Logical Minimum (0)
    0x25, 0x01,         //   Logical Maximum (1)
    0x75, 0x01,         //   Report Size (1 bit)
    0x95, 0x08,         //   Report Count (8 bits for modifiers)
    0x81, 0x02,         //   Input (Data, Var, Abs)

    // Reserved byte (1 byte)
    0x75, 0x08,         //   Report Size (8 bits)
    0x95, 0x01,         //   Report Count (1)
    0x81, 0x01,         //   Input (Const)

    // Keycodes (6 bytes)
    0x75, 0x08,         //   Report Size (8 bits)
    0x95, 0x06,         //   Report Count (6 bytes for keycodes)
    0x15, 0x00,         //   Logical Minimum (0)
    0x25, 0x65,         //   Logical Maximum (101 keys)
    0x05, 0x07,         //   Usage Page (Keyboard/Keypad)
    0x19, 0x00,         //   Usage Minimum (0)
    0x29, 0x65,         //   Usage Maximum (101 keys)
    0x81, 0x00,         //   Input (Data, Array)

    0xC0                // End Collection
};

static int bt_app_gap_event(struct ble_gap_event *event, void *arg);

static int handle_hid_read(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
    ESP_LOGI(TAG, "Reading HID Info...");
    uint8_t hid_info[] = {0x11, 0x01, 0x00, 0x00}; // HID v1.11 (USB 2.0), not localized, no remote wake
    os_mbuf_append(ctxt->om, hid_info, sizeof(hid_info));
    return 0;
}

static int handle_report_map_read(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
    os_mbuf_append(ctxt->om, keyboard_report_map, sizeof(keyboard_report_map));
    return 0;
}

static int handle_hid_control_point_write(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
    uint8_t control = ctxt->om->om_data[0];
    if (control == 0x00) {
        ESP_LOGI(TAG, "Control - 0");
    } else if (control == 0x01) {
        ESP_LOGI(TAG, "Control - 1");
    }
    return 0;
}

static int handle_hid_input_report(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
    uint8_t report[8] = {0};

    report[0] = 0x00; // no modifier keys are pressed;
    report[2] = 0x04; // letter 'a';
    ESP_LOGI(TAG, "Sending input report");
    os_mbuf_append(ctxt->om, report, sizeof(report));
    return 0;
}

static int handle_hid_protocol_mode(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
    uint8_t protocol_mode = 0x01;
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        os_mbuf_append(ctxt->om, &protocol_mode, sizeof(protocol_mode));
    } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        protocol_mode = ctxt->om->om_data[0];
    }

    return 0;
}

static int handle_device_name_read(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
    os_mbuf_append(ctxt->om, ble_svc_gap_device_name(), strlen(ble_svc_gap_device_name()));
    return 0;
}

static int handle_device_appearence_read(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
    const uint16_t appearance_value = BLE_APPEARANCE_HID_KEYBOARD;
    os_mbuf_append(ctxt->om, &appearance_value, sizeof(appearance_value));
    return 0;
}

static int handle_device_firmware_revision_read(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
    os_mbuf_append(ctxt->om, BT_APP_FIRMWARE_REVISION, strlen(BT_APP_FIRMWARE_REVISION));
    return 0;
}

static int handle_device_manufacturer_read(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
    os_mbuf_append(ctxt->om, BT_APP_MANUFACTURER, strlen(BT_APP_MANUFACTURER));
    return 0;
}

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(0x180A),
        .characteristics = (struct ble_gatt_chr_def[]){
                {
                    .uuid = BLE_UUID16_DECLARE(0x2A00), // Device name
                    .flags = BLE_GATT_CHR_F_READ,
                    .access_cb = handle_device_name_read
                },
                {
                    .uuid = BLE_UUID16_DECLARE(0x2A01), // Appearence
                    .flags = BLE_GATT_CHR_F_READ,
                    .access_cb = handle_device_appearence_read
                },
                {
                    .uuid = BLE_UUID16_DECLARE(0x2A26), // Firmware Revision
                    .flags = BLE_GATT_CHR_F_READ,
                    .access_cb = handle_device_firmware_revision_read
                },
                {
                    .uuid = BLE_UUID16_DECLARE(0x2A29), // Manufacturer
                    .flags = BLE_GATT_CHR_F_READ,
                    .access_cb = handle_device_manufacturer_read
                },
            {0}
        }
    },
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(0x1812), // HID Device
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = BLE_UUID16_DECLARE(0x2A4A), // HID Information
                .flags = BLE_GATT_CHR_F_READ,
                .access_cb = handle_hid_read
            },
            {
                .uuid = BLE_UUID16_DECLARE(0x2A4B), // Report Map
                .flags = BLE_GATT_CHR_F_READ,
                .access_cb = handle_report_map_read
            },
            {
                .uuid = BLE_UUID16_DECLARE(0x2A4C), // HID Control Point
                .flags = BLE_GATT_CHR_F_READ,
                .access_cb = handle_hid_control_point_write
            },
            {
                .uuid = BLE_UUID16_DECLARE(0x2A4D), // Input Report
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .access_cb = handle_hid_input_report
            },
            {
                .uuid = BLE_UUID16_DECLARE(0x2A4E), // Protocol Mode
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE_NO_RSP,
                .access_cb = handle_hid_protocol_mode
            },
            {
                .uuid = BLE_UUID16_DECLARE(0x2A22), // Boot Input Report
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .access_cb = handle_hid_input_report
            },
            {0}
        },
    },
    {0}
};

static void bt_app_advertise() {
    struct ble_hs_adv_fields fields;
    const char *device_name = ble_svc_gap_device_name();

    memset(&fields, 0, sizeof(fields));
    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;
    ble_gap_adv_set_fields(&fields);

    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    if (ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, bt_app_gap_event, NULL) == 0) {
        ESP_LOGI(TAG, "Bluetooth advertising started...");
    } else {
        ESP_LOGE(TAG, "Failed to start bluetooth advertising!");
    }
}

static int bt_app_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "Bluetooth connection status: %s", event->connect.status == 0 ? "OK" : "FAILED");
            if (event->connect.status != 0) bt_app_advertise();
        break;
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Bluetooth disconnected");
            bt_app_advertise();
        break;
        default:
            ESP_LOGI(TAG, "Bluetooth event: %d", event->type);
        break;
    }

    return 0;
}

static void bt_app_on_sync() {
    if (ble_hs_id_infer_auto(false, &ble_addr_type) != 0) {
        ESP_LOGE(TAG, "Failed to find best address type!");
        esp_restart();
    }
    bt_app_advertise();
}

void host_task(void *args) {
    nimble_port_run();
}

void bt_app_init() {
    nimble_port_init();
    ble_svc_gap_device_name_set(BT_APP_DEVICE_NAME);
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_gatts_count_cfg(gatt_svcs);
    ble_gatts_add_svcs(gatt_svcs);
    ble_hs_cfg.sync_cb = bt_app_on_sync;
    nimble_port_freertos_init(host_task);
}
