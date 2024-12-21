//
// Created by Kok on 12/21/24.
//

#ifndef BT_DEVICE_HID_HANDLERS_H
#define BT_DEVICE_HID_HANDLERS_H

int handle_hid_read(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg);

int handle_report_map_read(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

int handle_hid_control_point_write(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

int handle_hid_input_report(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

int handle_hid_output_report(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

int handle_hid_protocol_mode(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

int handle_hid_input_report_descriptor(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

#endif //BT_DEVICE_HID_HANDLERS_H
