//
// Created by Kok on 12/21/24.
//

#ifndef BT_DEVICE_INFO_HANDLERS_H
#define BT_DEVICE_INFO_HANDLERS_H

#include <stdint.h>

int handle_device_name_read(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg);

int handle_device_appearence_read(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

int handle_device_firmware_revision_read(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

int handle_device_manufacturer_read(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

#endif //BT_DEVICE_INFO_HANDLERS_H
