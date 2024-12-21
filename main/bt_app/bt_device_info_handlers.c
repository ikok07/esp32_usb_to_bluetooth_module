//
// Created by Kok on 12/21/24.
//

#include <host/ble_gatt.h>
#include <host/ble_hs.h>
#include <services/gap/ble_svc_gap.h>

#include "bt_constants.h"

int handle_device_name_read(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg) {
    os_mbuf_append(ctxt->om, ble_svc_gap_device_name(), strlen(ble_svc_gap_device_name()));
    return 0;
}

int handle_device_appearence_read(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
    const uint16_t appearance_value = BLE_SVC_GAP_APPEARANCE_GEN_HID;
    os_mbuf_append(ctxt->om, &appearance_value, sizeof(appearance_value));
    return 0;
}

int handle_device_firmware_revision_read(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
    os_mbuf_append(ctxt->om, BT_APP_FIRMWARE_REVISION, strlen(BT_APP_FIRMWARE_REVISION));
    return 0;
}

int handle_device_manufacturer_read(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
    os_mbuf_append(ctxt->om, BT_APP_MANUFACTURER, strlen(BT_APP_MANUFACTURER));
    return 0;
}