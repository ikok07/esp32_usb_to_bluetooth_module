//
// Created by Kok on 11/30/24.
//

#ifndef USB_APP_H
#define USB_APP_H

#include <stdint.h>
#include <usb/usb_host.h>

#include "hid_host.h"

#define USB_APP_VBUS_GPIO                       21  // GPIO to monitor if usb is connected

#define CLASS_DRIVER_ACTION_OPEN_DEV    0x01
#define CLASS_DRIVER_ACTION_TRANSFER    0x02
#define CLASS_DRIVER_ACTION_CLOSE_DEV   0x03

#define USB_HID_CLASS                   0x03            // The interface class for a keyboard is defined as HID (Human Interface Device).
#define USB_BOOT_INTERFACE_SUBCLASS     0x01            // ypically set to 0x01 for keyboards or 0x00 for generic HID devices.
#define USB_KEYBOARD_PROTOCOL           0x01            // Specifies that the interface is used for keyboard input.

#define USB_BM_ATTRIBUTES_XFER_MASK 0x03                // Extracts the transfer type from the bmAttributes field of an endpoint descriptor.
#define USB_EP_DIR_IN 0x80                              // Checks if the endpoint direction is IN (device-to-host).

typedef enum {
    APP_EVENT_HID_HOST = 0
} usb_app_event_group_e;

typedef struct {
    usb_app_event_group_e event_group;
    struct {
        hid_host_device_handle_t handle;
        hid_host_driver_event_t event;
        void *arg;
    } hid_host_device;
} usb_app_event_queue_t;

typedef struct {
    enum key_state {
        KEY_STATE_PRESSED = 0x00,
        KEY_STATE_RELEASED = 0x01
    } state;
    uint8_t modifier;
    uint8_t key_code;
} key_event_t;

static const char *hid_proto_name_str[] = {
    "NONE",
    "KEYBOARD",
    "MOUSE"
};

void usb_init();

#endif //USB_APP_H
