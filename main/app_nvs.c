/*
 * app_nvs.c
 *
 *  Created on: 2024. gada 11. janv.
 *      Author: Strau
 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "app_nvs.h"
#include "wifi_app.h"

static const char TAG[] = "nvs";

const char app_nvs_sta_creds_namespace[] = "stacreds";

esp_err_t app_nvs_save_sta_creds(void){
    nvs_handle handle;
    esp_err_t esp_err = ESP_OK;

    ESP_LOGI(TAG, "app_nvs_save_sta_creds: Saving station mode credentials to flash");

    wifi_config_t *wifi_sta_config = wifi_app_get_wifi_config();

    if(wifi_sta_config) {
        esp_err = nvs_open(app_nvs_sta_creds_namespace, NVS_READWRITE, &handle);

        if(esp_err != ESP_OK) {
            printf("app_nvs_save_sta_creds: Error (%s) opening NVS handle!\n", esp_err_to_name(esp_err));
            return esp_err;
        }

        esp_err = nvs_set_blob(handle, "ssid",  wifi_sta_config->sta.ssid, WIFI_AP_SSID_LENGTH);
        if(esp_err != ESP_OK) {
            printf("app_nvs_save_sta_creds: Error (%s) setting SSID to NVS!\n", esp_err_to_name(esp_err));
            return esp_err;
        }

        esp_err = nvs_set_blob(handle, "password",  wifi_sta_config->sta.password, WIFI_AP_PASSWORD_LENGTH);
        if(esp_err != ESP_OK) {
            printf("app_nvs_save_sta_creds: Error (%s) setting Password to NVS!\n", esp_err_to_name(esp_err));
            return esp_err;
        }

        esp_err = nvs_commit(handle);
        if(esp_err != ESP_OK) {
            printf("app_nvs_save_sta_creds: Error (%s) Commiting credentials to NVS!\n", esp_err_to_name(esp_err));
            return esp_err;
        }

        nvs_close(handle);
        ESP_LOGI(TAG, "app_nvs_save_sta_creds: wrote wifi_sta_config: Station SSID: %s, Password: %s", wifi_sta_config->sta.ssid, wifi_sta_config->sta.password);
    }

    return ESP_OK;
}

bool app_nvs_load_sta_creds(void){
    nvs_handle handle;
    esp_err_t esp_err;

    ESP_LOGI(TAG, "app_nvs_load_sta_creds: Loading WiFi credentials from flash");

    if(nvs_open(app_nvs_sta_creds_namespace, NVS_READONLY, &handle) == ESP_OK) {
        wifi_config_t *wifi_sta_config = wifi_app_get_wifi_config();

        if(wifi_sta_config == NULL) {
            wifi_sta_config = (wifi_config_t*)malloc(sizeof(wifi_config_t));
        }

        memset(wifi_sta_config, 0x00, sizeof(wifi_config_t));

        size_t wifi_config_size = sizeof(wifi_config_t);
        // This is weird, why not use 'WIFI_AP_SSID_LENGTH'?
        uint8_t *wifi_config_buff = (uint8_t*)malloc(sizeof(uint8_t) * wifi_config_size);

        // SSID
        wifi_config_size = sizeof(wifi_sta_config->sta.ssid);
        esp_err = nvs_get_blob(handle, "ssid", wifi_config_buff, &wifi_config_size);

        if(esp_err != ESP_OK) {
            free(wifi_config_buff);
            printf("app_nvs_load_sta_creds: (%s) no station SSID found in NVS\n", esp_err_to_name(esp_err));
            return esp_err;
        }

        memcpy(wifi_sta_config->sta.ssid, wifi_config_buff, wifi_config_size);

        // Password
        wifi_config_size = sizeof(wifi_sta_config->sta.password);
        esp_err = nvs_get_blob(handle, "password", wifi_config_buff, &wifi_config_size);

        if(esp_err != ESP_OK) {
            free(wifi_config_buff);
            printf("app_nvs_load_sta_creds: (%s) no station Password found in NVS\n", esp_err_to_name(esp_err));
            return esp_err;
        }

        memcpy(wifi_sta_config->sta.password, wifi_config_buff, wifi_config_size);

        free(wifi_config_buff);
        nvs_close(handle);

        printf("app_nvs_load_sta_creds: SSID: %s, Password: %s", wifi_sta_config->sta.ssid, wifi_sta_config->sta.password);

        return wifi_sta_config->sta.ssid[0] != '\0';
    } else {
        return false;
    }
}

esp_err_t app_nvs_clear_sta_creds(void) {
    nvs_handle handle;
    esp_err_t esp_err;

    ESP_LOGI(TAG, "app_nvs_clear_sta_creds: Clearing STA credentials");

    esp_err = nvs_open(app_nvs_sta_creds_namespace, NVS_READWRITE, &handle);
    if (esp_err != ESP_OK) {
        printf("app_nvs_clear_sta_creds: Error (%s) opening NVS handle", esp_err_to_name(esp_err));
        return esp_err;
    }

    esp_err = nvs_erase_all(handle);
    if (esp_err != ESP_OK) {
        printf("app_nvs_clear_sta_creds: Error (%s) ereasing credentials", esp_err_to_name(esp_err));
        return esp_err;
    }

    esp_err = nvs_commit(handle);
    if (esp_err != ESP_OK) {
        printf("app_nvs_clear_sta_creds: Error (%s) NVS commit!", esp_err_to_name(esp_err));
        return esp_err;
    }

    nvs_close(handle);

    printf("app_nvs_clear_sta_creds: ESP_OK");

    return ESP_OK;
}

