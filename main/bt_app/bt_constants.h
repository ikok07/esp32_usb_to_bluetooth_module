//
// Created by Kok on 12/21/24.
//

#ifndef BT_CONSTANTS_H
#define BT_CONSTANTS_H

#define BT_APP_DEVICE_NAME              "USB Bluetooth Module"
#define BT_APP_FIRMWARE_REVISION        "0.0.1"
#define BT_APP_MANUFACTURER             "Kaloyan Stefanov"

#define BLE_DEVICE_INFO_SERVICE_UUID    0x180A
#define BLE_BATTERY_SERVICE_UUID        0x180F

#define BLE_APPEARANCE_HID_KEYBOARD     0x03C1 // 961
#define BLE_BOOT_PROTOCOL_MODE          0x00
#define BLE_REPORT_PROTOCOL_MODE        0x01

extern const char BT_TAG[];

#endif //BT_CONSTANTS_H
