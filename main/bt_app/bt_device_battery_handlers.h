//
// Created by Kok on 12/21/24.
//

#ifndef BT_DEVICE_BATTERY_HANDLERS_H
#define BT_DEVICE_BATTERY_HANDLERS_H

int handle_battery_level(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

#endif //BT_DEVICE_BATTERY_HANDLERS_H
