//
// Created by Kok on 12/21/24.
//

#include <host/ble_gatt.h>
#include <host/ble_hs_id.h>
#include <host/ble_hs.h>

#include "bt_constants.h"

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

int handle_hid_read(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg) {
    ESP_LOGI(BT_TAG, "Reading HID Info...");
    uint8_t hid_info[] = {0x11, 0x01, 0x00, 0x00}; // HID v1.11 (USB 2.0), not localized, no remote wake
    os_mbuf_append(ctxt->om, hid_info, sizeof(hid_info));
    return 0;
}

int handle_report_map_read(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
    ESP_LOGI(BT_TAG, "Reading Report Map...");
    os_mbuf_append(ctxt->om, keyboard_report_map, sizeof(keyboard_report_map));
    return 0;
}

int handle_hid_control_point_write(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
    ESP_LOGI(BT_TAG, "Writing control point...");
    uint8_t control = ctxt->om->om_data[0];
    if (control == BLE_BOOT_PROTOCOL_MODE) {
        ESP_LOGI(BT_TAG, "Control - 0");
    } else if (control == BLE_REPORT_PROTOCOL_MODE) {
        ESP_LOGI(BT_TAG, "Control - 1");
    }
    return 0;
}

int handle_hid_input_report(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
    uint8_t report[8] = {0};
    report[0] = 0x00; // no modifier keys are pressed;
    report[2] = 0x04; // letter 'a';

    ESP_LOGI(BT_TAG, "Sending input report");
    os_mbuf_append(ctxt->om, report, sizeof(report));
    return 0;
}

int handle_hid_output_report(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
    return 0;
}

int handle_hid_protocol_mode(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
    ESP_LOGI(BT_TAG, "Reading HID Protocol Mode...");
    uint8_t protocol_mode = 0x01;
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        os_mbuf_append(ctxt->om, &protocol_mode, sizeof(protocol_mode));
    } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        protocol_mode = ctxt->om->om_data[0];
    }

    return 0;
}

int handle_hid_input_report_descriptor(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
    return 0;
}