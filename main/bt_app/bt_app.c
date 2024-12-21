//
// Created by Kok on 12/1/24.
//


#include <esp_log.h>

#include <string.h>
#include <host/ble_gatt.h>
#include <host/ble_hs_id.h>
#include <host/ble_hs.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>

#include "bt_app.h"

#include <store/config/ble_store_config.h>

#include "bt_constants.h"
#include "bt_device_info_handlers.h"
#include "bt_device_hid_handlers.h"
#include "bt_device_battery_handlers.h"

static uint8_t ble_addr_type = 0;
static uint16_t bt_conn_handle;

static int bt_app_gap_event(struct ble_gap_event *event, void *arg);

const struct ble_gatt_chr_def input_report_characteristic = {
    .uuid = BLE_UUID16_DECLARE(0x2A4D), // Input Report
    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
    .access_cb = handle_hid_input_report,
    // .descriptors = (struct ble_gatt_dsc_def[]) {
    //     {
    //         .uuid = BLE_UUID16_DECLARE(0x2908), // Report Reference Descriptor
    //         .att_flags = BLE_ATT_F_READ,
    //         .access_cb = handle_hid_input_report_descriptor
    //     },
    //     {
    //         .uuid = BLE_UUID16_DECLARE(0x2908), // Client Characteristic Configuration Descriptor (CCCD)
    //         .att_flags = BLE_ATT_F_READ,
    //         .access_cb = handle_hid_input_report_descriptor
    //     }
    // }
};

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_DEVICE_INFO_SERVICE_UUID), // Device Information
        .characteristics = (struct ble_gatt_chr_def[]) {
                {
                    .uuid = BLE_UUID16_DECLARE(0x2A00), // Device name
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC,
                    .access_cb = handle_device_name_read
                },
                {
                    .uuid = BLE_UUID16_DECLARE(0x2A01), // Appearence
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC,
                    .access_cb = handle_device_appearence_read
                },
                {
                    .uuid = BLE_UUID16_DECLARE(0x2A26), // Firmware Revision
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC,
                    .access_cb = handle_device_firmware_revision_read
                },
                {
                    .uuid = BLE_UUID16_DECLARE(0x2A29), // Manufacturer
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC,
                    .access_cb = handle_device_manufacturer_read
                },
            {0}
        }
    },
    // {
    //     .type = BLE_GATT_SVC_TYPE_PRIMARY,
    //     .uuid = BLE_UUID16_DECLARE(0x1812), // HID Device
    //     .characteristics = (struct ble_gatt_chr_def[]) {
    //         {
    //             .uuid = BLE_UUID16_DECLARE(0x2A4A), // HID Information
    //             .flags = BLE_GATT_CHR_F_READ,
    //             .access_cb = handle_hid_read
    //         },
    //         {
    //             .uuid = BLE_UUID16_DECLARE(0x2A4B), // Report Map
    //             .flags = BLE_GATT_CHR_F_READ,
    //             .access_cb = handle_report_map_read,
    //         },
    //         {
    //             .uuid = BLE_UUID16_DECLARE(0x2A4C), // HID Control Point
    //             .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
    //             .access_cb = handle_hid_control_point_write
    //         },
    //         input_report_characteristic,
    //         {
    //             .uuid = BLE_UUID16_DECLARE(0x2A4E), // Protocol Mode
    //             .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE_NO_RSP,
    //             .access_cb = handle_hid_protocol_mode
    //         },
    //         {
    //             .uuid = BLE_UUID16_DECLARE(0x2A22), // Boot Input Report
    //             .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
    //             .access_cb = handle_hid_input_report,
    //         },
    //         {
    //             .uuid = BLE_UUID16_DECLARE(0x2A32), // Boot Output Report
    //             .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
    //             .access_cb = handle_hid_output_report,
    //         },
    //         {0}
    //     },
    // },
{
    .type = BLE_GATT_SVC_TYPE_PRIMARY,
    .uuid = BLE_UUID16_DECLARE(BLE_BATTERY_SERVICE_UUID), // Battery
    .characteristics = (struct ble_gatt_chr_def[]) {
             {
                 .uuid = BLE_UUID16_DECLARE(0x2A19), // Battery Level
                 .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_NOTIFY,
                 .access_cb = handle_battery_level
             },
         {0}
        }
    },
    {0}
};

static void bt_configure_security() {
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ESP_LOGI(BT_TAG, "Security Mode 1 Configured (Level 2)");
}

static void bt_app_advertise() {
    struct ble_hs_adv_fields fields;
    const char *device_name = ble_svc_gap_device_name();

    memset(&fields, 0, sizeof(fields));

    fields.flags = BLE_HS_ADV_F_DISC_GEN |
                       BLE_HS_ADV_F_BREDR_UNSUP;

    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;
    fields.appearance = BLE_APPEARANCE_HID_KEYBOARD;
    fields.uuids16 = (ble_uuid16_t[]) {
        BLE_UUID16_INIT(BLE_BATTERY_SERVICE_UUID)
    };
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;
    fields.tx_pwr_lvl_is_present = true;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    ble_gap_adv_set_fields(&fields);

    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    if (ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, bt_app_gap_event, NULL) == 0) {
        ESP_LOGI(BT_TAG, "Bluetooth advertising started...");
    } else {
        ESP_LOGE(BT_TAG, "Failed to start bluetooth advertising!");
    }
}

int bt_app_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(BT_TAG, "Bluetooth connection status: %s", event->connect.status == 0 ? "OK" : "FAILED");
            if (event->connect.status != 0) bt_app_advertise();
            else {
                bt_conn_handle = event->connect.conn_handle;
                int res;
                if ((res = ble_gap_security_initiate(bt_conn_handle)) != 0) {
                    ESP_LOGE(BT_TAG, "Failed to initiate secure connection! Error: %d", res);
                    return ble_gap_terminate(event->enc_change.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
                }
            }
        break;
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(BT_TAG, "Bluetooth disconnected");
            bt_conn_handle = 0;
            bt_app_advertise();
        break;
        case BLE_GAP_EVENT_SUBSCRIBE:
            ESP_LOGI(BT_TAG, "Bluetooth device notification subscriptions status: %d", event->subscribe.cur_notify);
        break;
        case BLE_GAP_EVENT_PARING_COMPLETE:
            ESP_LOGI(BT_TAG, "Bluetooth pairing complete!");
        break;
        case BLE_GAP_EVENT_AUTHORIZE:
            ESP_LOGI(BT_TAG, "BLE_GAP_EVENT_AUTHORIZE");
        break;
        case BLE_GAP_EVENT_ENC_CHANGE:
            struct ble_gap_conn_desc conn_desc;
            if (ble_gap_conn_find(event->enc_change.conn_handle, &conn_desc) != 0) {
                ESP_LOGE(BT_TAG, "Failed to get information about connection!");
                return 0;
            }
            if (conn_desc.sec_state.authenticated == 0 || conn_desc.sec_state.encrypted == 0 || conn_desc.sec_state.bonded == 0) {
                ESP_LOGE(BT_TAG, "Failed to secure connection! Terminating...");
                ble_store_util_delete_peer(&conn_desc.peer_id_addr);
                return ble_gap_terminate(event->enc_change.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
            }
            ESP_LOGI(BT_TAG, "Connection secured");
        break;
        default:
            ESP_LOGI(BT_TAG, "Bluetooth event: %d", event->type);
        break;
    }

    return 0;
}

static void bt_app_on_sync() {
    if (ble_hs_id_infer_auto(false, &ble_addr_type) != 0) {
        ESP_LOGE(BT_TAG, "Failed to find best address type!");
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

    bt_configure_security();
    ble_hs_cfg.sync_cb = bt_app_on_sync;
    nimble_port_freertos_init(host_task);
}

void bt_app_send_input_report() {
    ble_gatts_notify(bt_conn_handle, *input_report_characteristic.val_handle);
}