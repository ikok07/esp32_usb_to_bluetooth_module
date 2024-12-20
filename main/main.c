#include <nvs_flash.h>
#include "bt_app/bt_app.h"
#include "usb_app/usb_app.h"

int app_main(void) {
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // usb_init();
    bt_app_init();
    return 0;
}
