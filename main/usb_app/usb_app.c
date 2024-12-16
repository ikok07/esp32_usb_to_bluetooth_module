//
// Created by Kok on 11/30/24.
//

#include "usb_app.h"

#include <esp_log.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <usb/usb_host.h>

#include "hid_host.h"
#include "hid_usage_keyboard.h"
#include "tasks_common.h"

static const char TAG[] = "usb_app";

QueueHandle_t usb_app_event_queue = NULL;

static void daemon_task(void *args) {
    ESP_LOGI(TAG, "Starting USB daemon task...");
    TaskHandle_t *notify_task_handle = (TaskHandle_t *)args;

    usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    // Send signal that host is installed
    xTaskNotifyGive(notify_task_handle);

    // wait until HID driver is initialized
    if (!ulTaskNotifyTake(true, pdMS_TO_TICKS(USB_APP_HID_INIT_TIMEOUT_MS))) {
        ESP_LOGE(TAG, "TIMEOUT | HID Driver was not initialized in time!");
        esp_restart();
    }

    while (1) {
        uint32_t event_flags;

        esp_err_t err = usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to handle USB Host Library events");
            continue;
        }

        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_LOGI(TAG, "No more USB Clients!");
            if (ESP_OK == usb_host_device_free_all()) {
                ESP_LOGI(TAG, "All USB devices are freed!");
            } else {
                ESP_LOGI(TAG, "Waiting for ALL_FREE Event...");
            }
        }

        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            ESP_LOGI(TAG, "All devices disconnected!");
        }
    }

    ESP_LOGI(TAG, "No more USB Clients and Devices!");
    ESP_ERROR_CHECK(usb_host_uninstall());
}

static void hid_print_new_device_report_header(hid_protocol_t proto) {
    static hid_protocol_t prev_proto_output = -1;

    if (prev_proto_output != proto) {
        prev_proto_output = proto;
        printf("\r\n");
        if (proto == HID_PROTOCOL_KEYBOARD) {
            printf("Keyboard\r\n");
        }
        fflush(stdout);
    }
}

static bool hid_keyboard_is_modifier_shift(uint8_t modifier) {
    if ((modifier & HID_LEFT_SHIFT) == HID_LEFT_SHIFT || (modifier & HID_RIGHT_SHIFT) == HID_RIGHT_SHIFT) {
        return true;
    }

    return false;
}

static bool hid_keyboard_get_char(uint8_t modifier, uint8_t key_code, unsigned char *key_char) {
    uint8_t mod = (hid_keyboard_is_modifier_shift(modifier)) ? 1 : 0;

    if ((key_code >= HID_KEY_A) && (key_code <= HID_KEY_SLASH)) {
        *key_char = keycode2ascii[key_code][mod];
    } else {
        return false;
    }

    return true;
}

static void key_event_callback(key_event_t *key_event) {
    unsigned char key_char;

    hid_print_new_device_report_header(HID_PROTOCOL_KEYBOARD);

    if (key_event->state == KEY_STATE_PRESSED) {
        if (hid_keyboard_get_char(key_event->modifier, key_event->key_code, &key_char)) {
            ESP_LOGI(TAG, "%c", key_char);
        }
    }
}

static bool key_found(const uint8_t *const src,
                             uint8_t key,
                             unsigned int length)
{
    for (unsigned int i = 0; i < length; i++) {
        if (src[i] == key) {
            return true;
        }
    }
    return false;
}

static void hid_host_keyboard_report_callback(const uint8_t *const data, const size_t length) {
    hid_keyboard_input_report_boot_t *kb_report = (hid_keyboard_input_report_boot_t*) data;

    if (length < sizeof(hid_keyboard_input_report_boot_t)) {
        return;
    }

    static uint8_t prev_keys[HID_KEYBOARD_KEY_MAX] = { 0 };
    key_event_t key_event;

    for (int i = 0; i < HID_KEYBOARD_KEY_MAX; i++) {
        // Key has been released
        if (prev_keys[i] > HID_KEY_ERROR_UNDEFINED && !key_found(kb_report->key, prev_keys[i], HID_KEYBOARD_KEY_MAX)) {
            key_event.key_code = prev_keys[i];
            key_event.modifier = 0;
            key_event.state = KEY_STATE_RELEASED;
            key_event_callback(&key_event);
        }

        // Key has been pressed
        if (kb_report->key[i] > HID_KEY_ERROR_UNDEFINED && !key_found(prev_keys, kb_report->key[i], HID_KEYBOARD_KEY_MAX)) {
            key_event.key_code = kb_report->key[i];
            key_event.modifier = kb_report->modifier.val;
            key_event.state = KEY_STATE_PRESSED;
            key_event_callback(&key_event);
        }
    }

    memcpy(prev_keys, &kb_report->key, HID_KEYBOARD_KEY_MAX);
}

static void hid_host_interface_callback(
    hid_host_device_handle_t hid_device_handle,
    const hid_host_interface_event_t event,
    void *arg)
{
    esp_err_t err;
    uint8_t data[64] = { 0 };
    size_t data_length = 0;
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
        case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
            err = hid_host_device_get_raw_input_report_data(hid_device_handle, data, 64, &data_length);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to get raw report data from USB device!");
                return;
            }

            if (dev_params.sub_class == HID_SUBCLASS_BOOT_INTERFACE) {
                if (dev_params.proto == HID_PROTOCOL_KEYBOARD) {
                    hid_host_keyboard_report_callback(data, data_length);
                    // The bluetooth part...
                }
            } else {
                // TODO (mouse?)
            }
        break;
        case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HID Device, protocol '%s' DISCONNECTED", hid_proto_name_str[dev_params.proto]);
            ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
        break;
        case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
            ESP_LOGI(TAG, "HID Device, protocol '%s' TRANSFER_ERROR",
                 hid_proto_name_str[dev_params.proto]);
        break;
        default:
            ESP_LOGE(TAG, "HID Device, protocol '%s' Unhandled event",
                     hid_proto_name_str[dev_params.proto]);
        break;
    }
}

static void hid_host_device_event(
    hid_host_device_handle_t hid_device_handle,
    const hid_host_driver_event_t event,
    void *arg)
{
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
        case HID_HOST_DRIVER_EVENT_CONNECTED:
            ESP_LOGI(TAG, "HID Device, protocol '%s' CONNECTED", hid_proto_name_str[dev_params.proto]);

            const hid_host_device_config_t dev_config = {
                .callback = hid_host_interface_callback,
                .callback_arg = NULL
            };

            ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));
            if (dev_params.sub_class == HID_SUBCLASS_BOOT_INTERFACE) {
                hid_report_protocol_t protocol = HID_REPORT_PROTOCOL_REPORT;
                ESP_ERROR_CHECK(hid_class_request_get_protocol(hid_device_handle, &protocol));

                if (dev_params.proto == HID_PROTOCOL_KEYBOARD) {
                    ESP_ERROR_CHECK(hid_class_request_set_idle(hid_device_handle, 0, 0));
                }
            }
            ESP_ERROR_CHECK(hid_host_device_start(hid_device_handle));
            break;
        default:
            break;
    }
}

static void hid_host_device_callback(
    hid_host_device_handle_t hid_device_handle,
    const hid_host_driver_event_t event,
    void *arg)
{
    const usb_app_event_queue_t evt_queue = {
        .event_group = USB_APP_EVENT_HID_HOST,
        .hid_host_device = {
            .handle = hid_device_handle,
            .event = event,
            .arg = arg
        }
    };

    if (usb_app_event_queue) {
        xQueueSend(usb_app_event_queue, &evt_queue, 0);
    }
}

void usb_init() {
    TaskHandle_t daemon_task_handle = NULL;
    usb_app_event_queue = xQueueCreate(10, sizeof(usb_app_event_queue_t));

    // Initialize daemon
    const bool daemon_task_created = xTaskCreatePinnedToCore(
        daemon_task,
        "daemon_task",
        USB_APP_DEAMON_TASK_STACK_SIZE,
        xTaskGetCurrentTaskHandle(),
        USB_APP_DEAMON_TASK_PRIORITY,
        &daemon_task_handle,
        USB_APP_DEAMON_TASK_CORE_ID
    );
    if (!daemon_task_created) {
        ESP_LOGE(TAG, "Failed to create USB Daemon task!");
        esp_restart();
    }

    // wait until USB Host driver is initialized
    if (!ulTaskNotifyTake(false, pdMS_TO_TICKS(USB_APP_HID_INIT_TIMEOUT_MS))) {
        ESP_LOGE(TAG, "TIMEOUT | USB Host Driver was not initialized in time!");
        esp_restart();
    }

    const hid_host_driver_config_t hid_host_driver_config = {
        .create_background_task = true,
        .task_priority = USB_APP_HID_TASK_PRIORITY,
        .stack_size =  USB_APP_HID_TASK_STACK_SIZE,
        .core_id = USB_APP_HID_TASK_CORE_ID,
        .callback = hid_host_device_callback,
        .callback_arg = NULL
    };

    ESP_ERROR_CHECK(hid_host_install(&hid_host_driver_config));
    xTaskNotifyGive(daemon_task_handle);

    usb_app_event_queue_t evt_queue;
    while (1) {
        if (xQueueReceive(usb_app_event_queue, &evt_queue, portMAX_DELAY)) {
            switch (evt_queue.event_group) {
                case USB_APP_EVENT_HID_HOST:
                    hid_host_device_event(
                    evt_queue.hid_host_device.handle,
                    evt_queue.hid_host_device.event,
                    evt_queue.hid_host_device.arg);
                break;
            }
        }
    }

    ESP_LOGI(TAG, "USB Driver uninstall");
    xQueueReset(usb_app_event_queue);
    vQueueDelete(usb_app_event_queue);
}