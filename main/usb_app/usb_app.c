//
// Created by Kok on 11/30/24.
//

#include "usb_app.h"

#include <esp_log.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <usb/usb_host.h>
#include <usb/usb_types_ch9.h>

#include "tasks_common.h"

static const char TAG[] = "usb_app";

static void client_event_cb(const usb_host_client_event_msg_t *msg, void *args) {
    class_driver_control_t *class_driver_obj = (class_driver_control_t*)args;
    switch (msg->event) {
        case USB_HOST_CLIENT_EVENT_NEW_DEV:
            class_driver_obj->actions |= CLASS_DRIVER_ACTION_OPEN_DEV;
            class_driver_obj->dev_addr = msg->new_dev.address;
            break;
        case USB_HOST_CLIENT_EVENT_DEV_GONE:
            class_driver_obj->actions |= CLASS_DRIVER_ACTION_CLOSE_DEV;
            break;
        default:
            break;
    }
}

static void transfer_cb(usb_transfer_t *transfer) {
    class_driver_control_t *class_driver_obj = (class_driver_control_t*)transfer->context;
    printf("Transfer status %d, actual number of bytes transferred %d\n", transfer->status, transfer->actual_num_bytes);

    if (transfer->status != USB_TRANSFER_STATUS_COMPLETED) {
        ESP_LOGE(TAG, "Transmition failed. Error code: %d", transfer->status);
        return;
    }

    if (transfer->actual_num_bytes >= 8) {

        // Get the pressed modifier key (shirt, ctrl, etc.)
        const uint8_t modifier = transfer->data_buffer[0];
        ESP_LOGI(TAG, "MODIFIER KEY: 0x%02X", modifier);

        // Get the pressed normal key
        for (int i = 2; i < 8; i++) {
            const uint8_t key_code = transfer->data_buffer[i];
            ESP_LOGI(TAG, "PRESSED KEY: %d", key_code);
        }
    } else {
        ESP_LOGE(TAG, "Transmition failed! Transfer contains only %d bytes", transfer->actual_num_bytes);
    }
}

/*
 * Returns 0xFF if no address is found
 */
static uint8_t find_IN_endpoint_addr(usb_config_desc_t *config_desc, const usb_intf_desc_t *intf_desc, int *intf_index) {
    const usb_ep_desc_t *ep_desc;
    for (int i = 0; i < intf_desc->bNumEndpoints; i++) {
        ep_desc = usb_parse_endpoint_descriptor_by_index(intf_desc, i, config_desc->wTotalLength, intf_index);
        if (!ep_desc) {
            ESP_LOGE(TAG, "Failed to parse endpoint descriptor!");
            return 0xFF;
        }

        bool isIN = ep_desc->bEndpointAddress & USB_EP_DIR_IN &&
            (ep_desc->bmAttributes & USB_BM_ATTRIBUTES_XFER_MASK) == USB_BM_ATTRIBUTES_XFER_INT;

        if (isIN) {
            ESP_LOGI(TAG, "Interrupt IN endpoint found: 0x%02X", ep_desc->bEndpointAddress);
            return ep_desc->bEndpointAddress;
        }
    }

    ESP_LOGW(TAG, "No Interrupt IN endpoint found!");
    return 0xFF;
}

static usb_app_check_keyboard_result_t check_for_keyboard_device(class_driver_control_t *class_driver_obj) {
    usb_app_check_keyboard_result_t result = {
        .is_keyboard = false,
        .endpoint_IN_addr = 0xFF,
        .interface_number = 0xFF
    };
    usb_device_desc_t *device_desc;
    usb_config_desc_t *config_desc;
    const usb_intf_desc_t *intf_desc;
    uint8_t endpoint_IN_addr = 0xFF;

    esp_err_t err = usb_host_get_device_descriptor(class_driver_obj->device_handle, &device_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get device descriptor! %s", esp_err_to_name(err));
        return result;
    }

    err = usb_host_get_config_desc(class_driver_obj->client_handle, class_driver_obj->device_handle, 0, &config_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get config descriptor! %s", esp_err_to_name(err));
        return result;
    }

    for (int i = 0; i < config_desc->bNumInterfaces; i++) {
        intf_desc = usb_parse_interface_descriptor(config_desc, i, 0, NULL);
        const bool is_keyboard = intf_desc != NULL &&
            intf_desc->bInterfaceClass == USB_HID_CLASS &&
            intf_desc->bInterfaceSubClass == USB_BOOT_INTERFACE_SUBCLASS &&
            intf_desc->bInterfaceProtocol == USB_KEYBOARD_PROTOCOL;

        if (is_keyboard) {
            result.is_keyboard = true;
            result.interface_number = intf_desc->bInterfaceNumber;
            ESP_LOGI(TAG, "Found HID Keyboard at Interface %d", result.interface_number);
            if ((endpoint_IN_addr = find_IN_endpoint_addr(config_desc, intf_desc, &i)) == 0xFF) {
                ESP_LOGE(TAG, "Failed to find IN endpoint address!");
            }
            result.endpoint_IN_addr = endpoint_IN_addr;
            break;
        }
    }
    if (!result.is_keyboard) ESP_LOGE(TAG, "No HID Keyboard is found!");

    usb_host_free_config_desc(config_desc);
    return result;
}

static void daemon_task(void *args) {
    usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    // Send signal that host is installed
    xTaskNotifyGive(args);

    bool has_clients = true;
    bool has_devices = true;
    while (has_clients || has_devices) {
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
            has_clients = false;
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            ESP_LOGI(TAG, "No more USB Devices connected!");
            has_devices = false;
        }
    }

    ESP_LOGI(TAG, "No more USB Clients and Devices!");

    ESP_ERROR_CHECK(usb_host_uninstall());
    vTaskSuspend(NULL);
}

static void client_task(void *args) {
    class_driver_control_t class_driver_obj = {0};
    const usb_host_client_config_t client_config = {
        .is_synchronous = false,
        .max_num_event_msg = 3,
        .async = {
            .client_event_callback = client_event_cb,
            .callback_arg = &class_driver_obj
        }
    };
    ESP_ERROR_CHECK(usb_host_client_register(&client_config, &class_driver_obj.client_handle));
    usb_transfer_t *transfer;
    ESP_ERROR_CHECK(usb_host_transfer_alloc(8, 0, &transfer));

    bool has_device = true;
    while (has_device) {
        esp_err_t err = usb_host_client_handle_events(class_driver_obj.client_handle, portMAX_DELAY);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to handle USB Client event! %s", esp_err_to_name(err));
            continue;
        }

        // If device is connected
        if (class_driver_obj.actions & CLASS_DRIVER_ACTION_OPEN_DEV) {
            err = usb_host_device_open(class_driver_obj.client_handle, class_driver_obj.dev_addr, &class_driver_obj.device_handle);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to open device! %s", esp_err_to_name(err));
                break;
            }

            const usb_app_check_keyboard_result_t res = check_for_keyboard_device(&class_driver_obj);
            if (!res.is_keyboard || res.endpoint_IN_addr == 0xFF || res.interface_number == 0xFF) break;
            class_driver_obj.device_endpoint_IN_addr = res.endpoint_IN_addr;

            err = usb_host_interface_claim(class_driver_obj.client_handle, class_driver_obj.device_handle, res.interface_number, 0);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to claim interface! %s", esp_err_to_name(err));
                break;
            }
        }

        // On transfer
        if (class_driver_obj.actions & CLASS_DRIVER_ACTION_TRANSFER) {
            memset(transfer->data_buffer, 0x00, 8); // Clear the buffer
            transfer->num_bytes = 8;
            transfer->device_handle = class_driver_obj.device_handle;
            transfer->bEndpointAddress = class_driver_obj.device_endpoint_IN_addr;
            transfer->callback = transfer_cb;
            transfer->context = (void*)&class_driver_obj;
            usb_host_transfer_submit(transfer);
        }

        // If device is disconnected
        if (class_driver_obj.actions & CLASS_DRIVER_ACTION_CLOSE_DEV) {
            err = usb_host_interface_release(class_driver_obj.client_handle, class_driver_obj.device_handle, 1);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to release interface! %s", esp_err_to_name(err));
                break;
            }
            err = usb_host_device_close(class_driver_obj.client_handle, class_driver_obj.device_handle);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to close device! %s", esp_err_to_name(err));
                break;
            }
            has_device = false;
        }
    }

    usb_host_transfer_free(transfer);
    usb_host_client_deregister(class_driver_obj.client_handle);
    vTaskSuspend(NULL);
}

void usb_init() {

    // Initialize daemon
    xTaskCreatePinnedToCore(
        daemon_task,
        "daemon_task",
        USB_APP_DEAMON_TASK_STACK_SIZE,
        NULL,
        USB_APP_DEAMON_TASK_PRIORITY,
        NULL,
        USB_APP_DEAMON_TASK_CORE_ID
    );
    ulTaskNotifyTake(false, 1000);

    // Initialize client
    xTaskCreatePinnedToCore(
        client_task,
        "client_task",
        USB_APP_CLIENT_TASK_STACK_SIZE,
        NULL,
        USB_APP_CLIENT_TASK_PRIORITY,
        NULL,
        USB_APP_CLIENT_TASK_CORE_ID
    );
}
