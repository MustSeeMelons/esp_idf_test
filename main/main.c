#include "nvs_flash.h"
#include "esp_wifi.h"
#include "wifi_app.h"
#include "AM2320.h"
#include "wifi_reset_button.h"

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
        ESP_ERROR_CHECK(ret);
    }

    am2320_init();

    wifi_app_start();

    am2320_task_start();

    wifi_reset_button_config();
}
