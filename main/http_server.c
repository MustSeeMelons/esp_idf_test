/*
 * http_server.c
 *
 *  Created on: 2024. gada 3. janv.
 *      Author: Strau
 */
#include "esp_http_server.h"
#include "esp_log.h"

#include "http_server.h"
#include "tasks_common.h"
#include "wifi_app.h"
#include "esp_ota_ops.h"
#include "sys/param.h"
#include "esp_timer.h"
#include "AM2320.h"

#include "esp_wifi.h"

static const char TAG[] = "http_server";

static int g_wifi_connect_status = NONE;

static int g_fw_update_status = OTA_UPDATE_PENDING;

// Task handle
static httpd_handle_t http_server_handle = NULL;

static TaskHandle_t task_http_server_monitor = NULL;

static QueueHandle_t http_server_monitor_handle;

const esp_timer_create_args_t fw_update_reset_args = {
        .callback = &http_server_fw_update_reset_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "fw_update_reset"
};

esp_timer_handle_t fw_update_reset;

// Embeded web fiels
extern const uint8_t jquery_3_3_1_min_js_start[] asm("_binary_jquery_3_3_1_min_js_start");
extern const uint8_t jquery_3_3_1_min_js_end[] asm("_binary_jquery_3_3_1_min_js_end");
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t app_css_start[] asm("_binary_app_css_start");
extern const uint8_t app_css_end[] asm("_binary_app_css_end");
extern const uint8_t app_js_start[] asm("_binary_app_js_start");
extern const uint8_t app_js_end[] asm("_binary_app_js_end");
extern const uint8_t favicon_ico_start[] asm("_binary_favicon_ico_start");
extern const uint8_t favicon_ico_end[] asm("_binary_favicon_ico_end");

static void http_server_fw_update_reset_timer(void) {
    if(g_fw_update_status == OTA_UPDATE_SUCCESSFUL) {
        ESP_LOGI(TAG, "http_server_fw_update_reset_timer: FW udpate successful starting FW update reset timer");

        ESP_ERROR_CHECK(esp_timer_create(&fw_update_reset_args, &fw_update_reset));
        ESP_ERROR_CHECK(esp_timer_start_once(fw_update_reset, 8000000)); // 8 seconds in micros
    } else {
        ESP_LOGI(TAG, "http_server_fw_update_reset_timer: FW update was unsuccessful");
    }
}

static void http_server_monitor(void *parameter) {
    http_server_queue_message_t msg;

    for (;;) {
        if (xQueueReceive(http_server_monitor_handle, &msg, portMAX_DELAY)) {
            switch (msg.msgID) {
                case HTTP_MSG_WIFI_CONNECT_INIT:
                    ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_INIT");
                    g_wifi_connect_status = HTTP_WIFI_STATUS_CONNECTING;
                    break;
                case HTTP_MSG_WIFI_CONNEC_SUCCESS:
                    ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNEC_SUCCESS");
                    g_wifi_connect_status = HTTP_WIFI_STATUS_CONNECT_SUCCESS;
                    break;
                case HTTP_MSG_WIFI_CONNECT_FAIL:
                    ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_FAIL");
                    g_wifi_connect_status = HTTP_WIFI_STATUS_CONNECT_FAILED;
                    break;
                case HTTP_MSG_OTA_UPDATE_SUCCESSFUL:
                    ESP_LOGI(TAG, "HTTP_MSG_OTA_UPDATE_SUCCESSFUL");
                    g_fw_update_status = OTA_UPDATE_SUCCESSFUL;
                    http_server_fw_update_reset_timer();
                    break;
                case HTTP_MSG_OTA_UPDATE_FAILED:
                    ESP_LOGI(TAG, "HTTP_MSG_OTA_UPDATE_FAILED");
                    g_fw_update_status = OTA_UPDATE_FAILED;
                    break;
                case HTTP_MSG_USER_DISCONNECT:
                    ESP_LOGI(TAG, "HTTP_MSG_USER_DISCONNECT");
                    g_wifi_connect_status = HTTP_WIFI_STATUS_DISCONNECTED;
                    break;
                default:
                    break;
            }
        }
    }
}

static esp_err_t httpd_server_jquery_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "jquery requested");

    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req,
            (const char*) jquery_3_3_1_min_js_start,
            jquery_3_3_1_min_js_end - jquery_3_3_1_min_js_start);

    return ESP_OK;
}

static esp_err_t httpd_server_index_html_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "index.html requested");

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req,
            (const char*) index_html_start,
            index_html_end - index_html_start);

    return ESP_OK;
}

static esp_err_t httpd_server_app_css_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "app.css requested");

    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req,
            (const char*) app_css_start,
            app_css_end - app_css_start);

    return ESP_OK;
}

static esp_err_t httpd_server_app_js_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "app.js requested");

    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req,
            (const char*) app_js_start,
            app_js_end - app_js_start);

    return ESP_OK;
}

static esp_err_t httpd_server_favicon_ico_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "favicon.ico requested");

    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req,
            (const char*) favicon_ico_start,
            favicon_ico_end - favicon_ico_start);

    return ESP_OK;
}

static esp_err_t http_server_OTA_update_handler(httpd_req_t *req) {
    esp_ota_handle_t ota_handle;
    char ota_buff[1024];
    int content_length = req->content_len;
    int content_received = 0;
    int recv_len;
    bool is_req_body_started = false;
    bool flash_successful = false;

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

    do {
        // Read data for request
        if ((recv_len = httpd_req_recv(req, ota_buff,
                MIN(content_length, sizeof(ota_buff)))) < 0) {
            // Check for timeout
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                ESP_LOGI(TAG, "http_server_OTA_update_handler: Socket Timeout");
                continue; // Retry on timeout
            }
            ESP_LOGI(TAG, "http_server_OTA_update_handler: OTA other err %d", recv_len);

            return ESP_FAIL;
        }

        printf("http_server_OTA_update_handler: OTA RX: %d of %d\r", content_received, content_length);

        // Is this the first batch?
        if(!is_req_body_started) {
            is_req_body_started = true;

            // Get the location of the .bin file content (remove web form data)
            char *body_start_p = strstr(ota_buff, "\r\n\r\n") + 4;
            int body_part_len = recv_len - (body_start_p - ota_buff);

            printf("http_server_OTA_update_handler: OTA file size: %d\r\n", content_length);

            esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);

            if(err != ESP_OK) {
                printf("http_server_OTA_update_handler: Error with OTA begin, cancelling OTA.");

                return ESP_FAIL;
            } else {
                ESP_LOGI(TAG,
                    "http_server_OTA_update_handler: Writing to partition subtype %d at offset 0x%p\r\n",
                    update_partition->subtype,
                    (void*) update_partition->address
                );
            }

            esp_ota_write(ota_handle, body_start_p, body_part_len);
            content_received += body_part_len;
        } else {
            esp_ota_write(ota_handle, ota_buff, recv_len);
            content_received += recv_len;
        }


    } while (recv_len > 0 && content_received < content_length);

    if (esp_ota_end(ota_handle) == ESP_OK) {
        if (esp_ota_set_boot_partition(update_partition) == ESP_OK) {
            const esp_partition_t *boot_partition = esp_ota_get_boot_partition();

            ESP_LOGI(
                    TAG,
                    "http_server_OTA_update_handler: Next boot partition subtype %d at offset 0x%p",
                    boot_partition->subtype,
                    (void*) boot_partition->address
            );

            flash_successful = true;
        } else {
            ESP_LOGI(TAG, "http_server_OTA_update_handler: FLASHED ERROR!!!");
        }
    } else {
        ESP_LOGI(TAG, "http_server_OTA_update_handler: esp_ota_end poo!!!");
    }

    if (flash_successful) {
        http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_SUCCESSFUL);
    } else {
        http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_FAILED);
    }

    return ESP_OK;
}

static esp_err_t http_server_OTA_status_handler(httpd_req_t *req) {
    char otaJSON[100];

    ESP_LOGI(TAG, "OTAstatus requested.");

    sprintf(otaJSON, "{\"ota_update_status\":%d,\"compile_time\":\"%s\",\"compile_date\":\"%s\"}", g_fw_update_status, __TIME__, __DATE__);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, otaJSON, strlen(otaJSON));

    return ESP_OK;
}

static esp_err_t http_server_am2320_handler(httpd_req_t *req) {
    char am2320JSON[100];

    ESP_LOGI(TAG, "AM2320.json requested.");

    sprintf(am2320JSON, "{\"temp\":%.2f,\"humidity\":\"%.2f\"}", get_temprature(), get_humidity());

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, am2320JSON, strlen(am2320JSON));

    return ESP_OK;
}

static esp_err_t http_server_wifi_connect_json_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "/wifiConnect.json requested");

    size_t len_ssid = 0, len_pass = 0;

    char *ssid_str = NULL, *pass_str = NULL;

    len_ssid = httpd_req_get_hdr_value_len(req, "my-connect-ssid") + 1;
    if(len_ssid > 1) {
        ssid_str = malloc(len_ssid);
        if(httpd_req_get_hdr_value_str(req, "my-connect-ssid", ssid_str, len_ssid) == ESP_OK) {
            ESP_LOGI(TAG, "http_server_wifi_connect_json_handler: Found header => 'my-connect-ssid: %s'", ssid_str);
        }
    }

    len_pass = httpd_req_get_hdr_value_len(req, "my-connect-pass") + 1;
    if(len_pass > 1) {
        pass_str = malloc(len_pass);
        if(httpd_req_get_hdr_value_str(req, "my-connect-pass", pass_str, len_pass) == ESP_OK) {
            ESP_LOGI(TAG, "http_server_wifi_connect_json_handler: Found header => 'my-connect-pass: %s'", pass_str);
        }
    }

    wifi_config_t *wifi_config = wifi_app_get_wifi_config();
    memset(wifi_config, 0x00, sizeof(wifi_config_t));
    memcpy(wifi_config->sta.ssid, ssid_str, len_ssid);
    memcpy(wifi_config->sta.password, pass_str, len_pass);

    wifi_app_send_message(WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER);

    free(ssid_str);
    free(pass_str);

    return ESP_OK;
}

static esp_err_t http_server_wifi_connect_status_json_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "/wifiConnectStatus requested");

    char statusJSON[100];

    sprintf(statusJSON, "{\"wifi_connect_status\":%d}", g_wifi_connect_status);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, statusJSON, strlen(statusJSON));

    return ESP_OK;
}

static esp_err_t http_server_wifi_disconnect_json_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "/wifiDisconnect requested");

    wifi_app_send_message(WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT);

    return ESP_OK;
}

static esp_err_t http_server_get_wifi_connect_info_json_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "/wifiConnectInfo requested");

    char wifiInfoJSON[200];
    memset(wifiInfoJSON, 0, sizeof(wifiInfoJSON));

    char ip[IP4ADDR_STRLEN_MAX];
    char netmask[IP4ADDR_STRLEN_MAX];
    char gw[IP4ADDR_STRLEN_MAX];

    if(g_wifi_connect_status == HTTP_WIFI_STATUS_CONNECT_SUCCESS) {
        wifi_ap_record_t wifi_data;
        ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&wifi_data));

        char *ssid = (char*) wifi_data.ssid;
        esp_netif_ip_info_t ip_info;
        ESP_ERROR_CHECK(esp_netif_get_ip_info(esp_netif_sta, &ip_info));

        esp_ip4addr_ntoa(&ip_info.ip, ip, IP4ADDR_STRLEN_MAX);
        esp_ip4addr_ntoa(&ip_info.netmask, netmask, IP4ADDR_STRLEN_MAX);
        esp_ip4addr_ntoa(&ip_info.gw, gw, IP4ADDR_STRLEN_MAX);

        sprintf(
                wifiInfoJSON,
                "{\"ip\": \"%s\", \"netmask\": \"%s\", \"gw\": \"%s\", \"ap\": \"%s\"}",
                ip,
                netmask,
                gw,
                ssid
        );

        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, wifiInfoJSON, strlen(wifiInfoJSON));
    }

    return ESP_OK;
}

static httpd_handle_t http_server_configure(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    xTaskCreatePinnedToCore(
            &http_server_monitor,
            "http_server_monitor",
            HTTP_SERVER_MONITOR_STACK_SIZE,
            NULL,
            HTTP_SERVER_MONITOR_PRIORITY,
            &task_http_server_monitor,
            HTTP_SERVER_MONITOR_CORE_ID
    );

    http_server_monitor_handle = xQueueCreate(3, sizeof(http_server_queue_message_t));

    config.core_id = HTTP_SERVER_TASK_CORE_ID;
    config.task_priority = HTTP_SERVER_TASK_PRIORITY;
    config.stack_size = HTTP_SERVER_STACK_SIZE;
    config.max_uri_handlers = 20;
    config.recv_wait_timeout = 10; // seconds
    config.send_wait_timeout = 10;

    ESP_LOGI(TAG,
            "http_server_configure: Starting server on port: '%d' with task priority: '%d'",
            config.server_port, config.task_priority);

    if (httpd_start(&http_server_handle, &config) == ESP_OK) {
        ESP_LOGI(TAG, "http_server_configure: Registering URI handlers");

        // @formatter:off
        httpd_uri_t jqueary_js = {
                .uri = "/jquery-3.3.1.min.js",
                .method = HTTP_GET,
                .handler = httpd_server_jquery_handler,
                .user_ctx = NULL
        };

        httpd_register_uri_handler(http_server_handle, &jqueary_js);

        httpd_uri_t index_html = {
               .uri = "/",
               .method = HTTP_GET,
               .handler = httpd_server_index_html_handler,
               .user_ctx = NULL
       };

        httpd_register_uri_handler(http_server_handle, &index_html);

        httpd_uri_t app_css = {
              .uri = "/app.css",
              .method = HTTP_GET,
              .handler = httpd_server_app_css_handler,
              .user_ctx = NULL
        };

        httpd_register_uri_handler(http_server_handle, &app_css);

        httpd_uri_t app_js = {
            .uri = "/app.js",
            .method = HTTP_GET,
            .handler = httpd_server_app_js_handler,
            .user_ctx = NULL
        };

        httpd_register_uri_handler(http_server_handle, &app_js);

        httpd_uri_t favicon_ico = {
            .uri = "/favicon.ico",
            .method = HTTP_GET,
            .handler = httpd_server_favicon_ico_handler,
            .user_ctx = NULL
        };

        httpd_register_uri_handler(http_server_handle, &favicon_ico);

        httpd_uri_t OTA_update = {
            .uri = "/OTAupdate",
            .method = HTTP_POST,
            .handler = http_server_OTA_update_handler,
            .user_ctx = NULL
        };

        httpd_register_uri_handler(http_server_handle, &OTA_update);

        httpd_uri_t OTA_status = {
            .uri = "/OTAstatus",
            .method = HTTP_POST,
            .handler = http_server_OTA_status_handler,
            .user_ctx = NULL
        };

        httpd_register_uri_handler(http_server_handle, &OTA_status);

        httpd_uri_t AM2320 = {
            .uri = "/am2320Sensor.json",
            .method = HTTP_GET,
            .handler = http_server_am2320_handler,
            .user_ctx = NULL
        };

        httpd_register_uri_handler(http_server_handle, &AM2320);

        httpd_uri_t wifi_connect_json = {
            .uri = "/wifiConnect.json",
            .method = HTTP_POST,
            .handler = http_server_wifi_connect_json_handler,
            .user_ctx = NULL
        };

        httpd_register_uri_handler(http_server_handle, &wifi_connect_json);

        httpd_uri_t wifi_connect_status_json = {
            .uri = "/wifiConnectStatus.json",
            .method = HTTP_POST,
            .handler = http_server_wifi_connect_status_json_handler,
            .user_ctx = NULL
        };

        httpd_register_uri_handler(http_server_handle, &wifi_connect_status_json);

        httpd_uri_t wifi_connect_info_json = {
            .uri = "/wifiConnectInfo.json",
            .method = HTTP_GET,
            .handler = http_server_get_wifi_connect_info_json_handler,
            .user_ctx = NULL
        };

        httpd_register_uri_handler(http_server_handle, &wifi_connect_info_json);

        httpd_uri_t wifi_disconnect_json = {
            .uri = "/wifiDisconnect.json",
            .method = HTTP_DELETE,
            .handler = http_server_wifi_disconnect_json_handler,
            .user_ctx = NULL
        };

        httpd_register_uri_handler(http_server_handle, &wifi_disconnect_json);

        // @formatter:on

        return http_server_handle;
    }

    return NULL;

}

void http_server_start(void) {
    if (http_server_handle == NULL) {
        http_server_handle = http_server_configure();
    }
}

void http_server_stop(void) {
    if (http_server_handle) {
        httpd_stop(http_server_handle);
        ESP_LOGI(TAG, "http_server_stop: stopping HTTP server");
        http_server_handle = NULL;
    }

    if(task_http_server_monitor) {
        vTaskDelete(task_http_server_monitor);
        ESP_LOGI(TAG, "http_server_stop: stopping HTTP server monitor");
        task_http_server_monitor = NULL;
    }
}

BaseType_t http_server_monitor_send_message(http_server_message_e msgID) {
    http_server_queue_message_t msg;
    msg.msgID = msgID;

    return xQueueSend(http_server_monitor_handle, &msg, portMAX_DELAY);
}

void http_server_fw_update_reset_callback(void *arg) {
    ESP_LOGI(TAG, "http_server_fw_update_reset_callback: Timer timed-out, restarting the device.");

    esp_restart();
}
