//
// Created by Kok on 12/21/24.
//

#include <stdint.h>
#include <host/ble_gatt.h>
#include <os/os_mbuf.h>

int handle_battery_level(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg) {
    const uint8_t battery_level = 100;

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        os_mbuf_append(ctxt->om, &battery_level, sizeof(battery_level));
    }
    return 0;
}
