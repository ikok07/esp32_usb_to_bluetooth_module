//
// Created by Kok on 11/30/24.
//

#ifndef USB_APP_H
#define USB_APP_H

#include <stdint.h>
#include <usb/usb_host.h>

#define USB_APP_VBUS_GPIO                       21  // GPIO to monitor if usb is connected

#define CLASS_DRIVER_ACTION_OPEN_DEV    0x01
#define CLASS_DRIVER_ACTION_TRANSFER    0x02
#define CLASS_DRIVER_ACTION_CLOSE_DEV   0x03

#define USB_HID_CLASS                   0x03            // The interface class for a keyboard is defined as HID (Human Interface Device).
#define USB_BOOT_INTERFACE_SUBCLASS     0x01            // ypically set to 0x01 for keyboards or 0x00 for generic HID devices.
#define USB_KEYBOARD_PROTOCOL           0x01            // Specifies that the interface is used for keyboard input.

#define USB_BM_ATTRIBUTES_XFER_MASK 0x03                // Extracts the transfer type from the bmAttributes field of an endpoint descriptor.
#define USB_EP_DIR_IN 0x80                              // Checks if the endpoint direction is IN (device-to-host).


typedef struct {
    uint32_t actions;
    uint8_t dev_addr;
    usb_host_client_handle_t client_handle;
    usb_device_handle_t device_handle;
    uint8_t device_endpoint_IN_addr;
} class_driver_control_t;

typedef struct {
    bool is_keyboard;
    uint8_t endpoint_IN_addr;
    uint8_t interface_number;
} usb_app_check_keyboard_result_t;

void usb_init();

#endif //USB_APP_H
