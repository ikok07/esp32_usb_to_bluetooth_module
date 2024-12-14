//
// Created by Kok on 12/1/24.
//

#include "bt_app.h"

#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_err.h>
#include <esp_gap_ble_api.h>
#include <esp_log.h>

#include "esp_hidd_prf_api.h"

static const char TAG[] = "bt_app";

static uint16_t conn_id = 0;
static bool secure_conn = false;

static uint8_t hidd_service_uuid128[] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x12, 0x18, 0x00, 0x00,
};

static esp_ble_adv_data_t hidd_adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006, //slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval = 0x0010, //slave connection max interval, Time = max_interval * 1.25 msec
    .appearance = 0x03c0,       //HID Generic,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(hidd_service_uuid128),
    .p_service_uuid = hidd_service_uuid128,
    .flag = 0x6,
};

static esp_ble_adv_params_t hidd_adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x30,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    //.peer_addr            =
    //.peer_addr_type       =
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void hidd_event_callback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param) {
    switch (event) {
        case ESP_HIDD_EVENT_REG_FINISH:
            ESP_LOGI(TAG, "HID Profile registered!");
            esp_ble_gap_set_device_name(BT_APP_DEVICE_NAME);
            esp_ble_gap_config_adv_data(&hidd_adv_data);
        case ESP_HIDD_EVENT_BLE_CONNECT:
            ESP_LOGI(TAG, "ESP is connected to bluetooth device");
            conn_id = param->connect.conn_id;
            break;
        case ESP_HIDD_EVENT_BLE_DISCONNECT:
            secure_conn = false;
            ESP_LOGI(TAG, "ESP is disconnected to bluetooth device. Starting advertising...");
            esp_ble_gap_start_advertising(&hidd_adv_params);
        break;
        case ESP_HIDD_EVENT_BLE_LED_REPORT_WRITE_EVT:
            ESP_LOGI(TAG, "ESP_HIDD_EVENT_BLE_LED_REPORT_WRITE_EVT");
            // To be developed...
            break;
        case ESP_BAT_EVENT_REG:
            break;
        case ESP_HIDD_EVENT_DEINIT_FINISH:
            break;
        default:
            break;;
    }
}

void bt_app_init() {
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_controller_config = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_bt_controller_init(&bt_controller_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Bluetooth controller! %s", esp_err_to_name(err));
        esp_restart();
    }

    if ((err = esp_bt_controller_enable(ESP_BT_MODE_BLE)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable BLE! %s", esp_err_to_name(err));
        esp_restart();
    }

    if ((err = esp_bluedroid_init()) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Bluedroid! %s", esp_err_to_name(err));
        esp_restart();
    }

    if ((err = esp_bluedroid_enable()) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable Bluedroid! %s", esp_err_to_name(err));
        esp_restart();
    }

    if ((err = esp_hidd_profile_init()) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize HIDD Profile! %s", esp_err_to_name(err));
        esp_restart();
    }

    esp_hidd_register_callbacks(hidd_event_callback);
}
