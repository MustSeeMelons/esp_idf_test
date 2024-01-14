/*
 * wifi_app.c
 *
 *  Created on: 2024. gada 2. janv.
 *      Author: Strau
 */
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_ds_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "lwip/netdb.h"

#include "rgb_led.h"
#include "tasks_common.h"
#include "wifi_app.h"
#include "http_server.h"

#include "app_nvs.h"

static const char TAG[] = "wifi_app";

wifi_config_t *wifi_config = NULL;

static int g_retry_number;

static EventGroupHandle_t wifi_app_event_group;
const int WIFI_APP_CONNECTING_USING_SAVED_CREDS_BIT = BIT0;
const int WIFI_APP_CONNECTING_FROM_HTTP_SERVER_BIT = BIT1;
const int WIFI_APP_USER_REQUESTED_STA_DISCONNECT_BIT = BIT2;
const int WIFI_APP_STA_CONNECTED_GOT_IP_BIT = BIT3;

static QueueHandle_t wifi_app_queue_handle;

esp_netif_t *esp_netif_sta = NULL;
esp_netif_t *esp_netif_ap = NULL;

static void wifi_app_event_handler(void *arg, esp_event_base_t event_base,
		int32_t event_id, void *event_data) {
	if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "WIFI_EVENT_AP_START");
                break;
            case WIFI_EVENT_AP_STOP:
                ESP_LOGI(TAG, "WIFI_EVENT_AP_STOP");
                break;
            case WIFI_EVENT_AP_STACONNECTED:
                ESP_LOGI(TAG, "WIFI_EVENT_AP_STACONNECTED");
                break;
            case WIFI_EVENT_AP_STADISCONNECTED:
                ESP_LOGI(TAG, "WIFI_EVENT_AP_STADISCONNECTED");
                break;
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");

                wifi_event_sta_disconnected_t *wifi_event_sta_disconnected = (wifi_event_sta_disconnected_t*)malloc(sizeof(wifi_event_sta_disconnected_t));
                *wifi_event_sta_disconnected = *((wifi_event_sta_disconnected_t*)event_data);
                printf("WIFI_EVENT_STA_DISCONNECTED, reason code %d\n", wifi_event_sta_disconnected->reason);

                if(g_retry_number < 3) {
                    esp_wifi_connect();
                    g_retry_number++;
                } else {
                    wifi_app_send_message(WIFI_APP_MSG_STA_DISCONNECTED);
                }
			break;
		}
	} else if (event_base == IP_EVENT) {
		switch (event_id) {
            case IP_EVENT_STA_GOT_IP:
                ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP");
                wifi_app_send_message(WIFI_APP_MSG_STA_CONNECTED_GOT_IP);
                break;
		}
	}
}

static void wifi_app_event_handler_init(void) {
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	esp_event_handler_instance_t instance_wifi_event;
	esp_event_handler_instance_t instance_ip_event;

	ESP_ERROR_CHECK(
			esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_app_event_handler, NULL, &instance_wifi_event));

	ESP_ERROR_CHECK(
			esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_app_event_handler, NULL, &instance_ip_event));
}

static void wifi_app_default_wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    esp_netif_sta = esp_netif_create_default_wifi_sta();
    esp_netif_ap = esp_netif_create_default_wifi_ap();

}

static void wifi_app_soft_ap_config(void) {
    // @formatter:off
    wifi_config_t ap_config = {
            .ap = {
                    .ssid = WIFI_AP_SSID,
                    .ssid_len = strlen(WIFI_AP_SSID),
                    .password = WIFI_AP_PASSWORD,
                    .channel = WIFI_AP_CHANNEL,
                    .ssid_hidden = WIFI_AP_SSID_HIDDEN,
                    .authmode = WIFI_AUTH_WPA2_PSK,
                    .max_connection = WIFI_AP_MAX_CONNECTIONS,
                    .beacon_interval = WIFI_AP_BEACON_INTERVAL,
            },
    };
        // @formatter:on

    esp_netif_ip_info_t ap_ip_info;
    memset(&ap_ip_info, 0x00, sizeof(ap_ip_info));

    esp_netif_dhcps_stop(esp_netif_ap);

    inet_pton(AF_INET, WIFI_AP_IP, &ap_ip_info.ip);
    inet_pton(AF_INET, WIFI_AP_GATEWAY, &ap_ip_info.gw);
    inet_pton(AF_INET, WIFI_AP_NETMASK, &ap_ip_info.netmask);

    ESP_ERROR_CHECK(esp_netif_set_ip_info(esp_netif_ap, &ap_ip_info));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(esp_netif_ap));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(ESP_IF_WIFI_AP, WIFI_AP_BANDWIDTH ));

    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_AP_POWER_SAVE));

}

static void wifi_app_connect_sta(void) {
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_app_get_wifi_config()));
    ESP_ERROR_CHECK(esp_wifi_connect());
}

static void wifi_app_task(void *pvParameters) {
	wifi_app_queue_message_t msg;
	EventBits_t eventBits;

	wifi_app_event_handler_init();
	wifi_app_default_wifi_init();
	wifi_app_soft_ap_config();

	ESP_ERROR_CHECK(esp_wifi_start());

	wifi_app_send_message(WIFI_APP_MSG_LOAD_SAVED_CREDS);

    for (;;) {
        if (xQueueReceive(wifi_app_queue_handle, &msg, portMAX_DELAY)) {
            switch (msg.msgID) {
                case WIFI_APP_MSG_LOAD_SAVED_CREDS:
                    ESP_LOGI(TAG, "WIFI_APP_MSG_LOAD_SAVED_CREDS");

                    if(app_nvs_load_sta_creds()) {
                        ESP_LOGI(TAG, "Loaded station configuration");
                        wifi_app_connect_sta();

                        xEventGroupSetBits(wifi_app_event_group, WIFI_APP_CONNECTING_USING_SAVED_CREDS_BIT);
                    } else {
                        ESP_LOGI(TAG, "Unavle to load station configuration");
                    }

                    wifi_app_send_message(WIFI_APP_MSG_START_HTTP_SERVER);

                    break;
                case WIFI_APP_MSG_START_HTTP_SERVER:
                    ESP_LOGI(TAG, "WIFI_APP_MSG_START_HTTP_SERVER");
                    http_server_start();
                    rgb_led_http_started();
                    break;
                case WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER:
                    ESP_LOGI(TAG, "WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER");

                    xEventGroupSetBits(wifi_app_event_group, WIFI_APP_CONNECTING_FROM_HTTP_SERVER_BIT);

                    wifi_app_connect_sta();

                    g_retry_number = 0;

                    http_server_monitor_send_message(
                            HTTP_MSG_WIFI_CONNECT_INIT);

                    break;
                case WIFI_APP_MSG_STA_CONNECTED_GOT_IP:
                    ESP_LOGI(TAG, "WIFI_APP_MSG_STA_CONNECTED_GOT_IP");

                    xEventGroupSetBits(wifi_app_event_group, WIFI_APP_STA_CONNECTED_GOT_IP_BIT);

                    rgb_led_wifi_connected();
                    http_server_monitor_send_message(HTTP_MSG_WIFI_CONNEC_SUCCESS);

                    eventBits = xEventGroupGetBits(wifi_app_event_group);

                    if(eventBits & WIFI_APP_CONNECTING_USING_SAVED_CREDS_BIT) {
                        xEventGroupClearBits(wifi_app_event_group, WIFI_APP_CONNECTING_USING_SAVED_CREDS_BIT);
                    } else {
                        app_nvs_save_sta_creds();
                    }

                    if(eventBits & WIFI_APP_CONNECTING_FROM_HTTP_SERVER_BIT) {
                        xEventGroupClearBits(wifi_app_event_group, WIFI_APP_CONNECTING_FROM_HTTP_SERVER_BIT);
                    }

                    break;
                case WIFI_APP_MSG_STA_DISCONNECTED:
                    ESP_LOGI(TAG, "WIFI_APP_MSG_STA_DISCONNECTED");

                    eventBits = xEventGroupGetBits(wifi_app_event_group);

                    if(eventBits & WIFI_APP_CONNECTING_USING_SAVED_CREDS_BIT) {
                        ESP_LOGI(TAG, "Attempt using saved credentials.");
                        xEventGroupClearBits(wifi_app_event_group, WIFI_APP_CONNECTING_USING_SAVED_CREDS_BIT);
                        app_nvs_clear_sta_creds();
                    } else if(eventBits & WIFI_APP_CONNECTING_FROM_HTTP_SERVER_BIT) {
                        ESP_LOGI(TAG, "WIFI_APP_MSG_STA_DISCONNECTED: Attempt from HTTP server.");
                        xEventGroupClearBits(wifi_app_event_group, WIFI_APP_CONNECTING_FROM_HTTP_SERVER_BIT);
                        http_server_monitor_send_message(HTTP_MSG_WIFI_CONNECT_FAIL);
                    } else if(eventBits & WIFI_APP_USER_REQUESTED_STA_DISCONNECT_BIT) {
                        ESP_LOGI(TAG, "WIFI_APP_MSG_STA_DISCONNECTED: User reqeusted disconnect.");
                        xEventGroupClearBits(wifi_app_event_group, WIFI_APP_USER_REQUESTED_STA_DISCONNECT_BIT);
                        http_server_monitor_send_message(HTTP_MSG_USER_DISCONNECT);
                    } else {
                        ESP_LOGI(TAG, "WIFI_APP_MSG_STA_DISCONNECTED: Attempt failed, check wifi AP availability.");
                    }

                    if(eventBits & WIFI_APP_STA_CONNECTED_GOT_IP_BIT) {
                        xEventGroupClearBits(wifi_app_event_group, WIFI_APP_STA_CONNECTED_GOT_IP_BIT);
                    }

                    break;
                case WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT:
                    ESP_LOGI(TAG, "WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT");

                    eventBits = xEventGroupGetBits(wifi_app_event_group);

                    if (eventBits & WIFI_APP_STA_CONNECTED_GOT_IP_BIT) {
                        xEventGroupSetBits(wifi_app_event_group, WIFI_APP_USER_REQUESTED_STA_DISCONNECT_BIT);

                        g_retry_number = 3;
                        ESP_ERROR_CHECK(esp_wifi_disconnect());
                        rgb_led_http_started();
                        app_nvs_clear_sta_creds();
                    }
                    break;
                default:
                    break;
            }
        }
	}
}

BaseType_t wifi_app_send_message(wifi_app_message_e msgID) {
	wifi_app_queue_message_t msg;
	msg.msgID = msgID;

	return xQueueSend(wifi_app_queue_handle, &msg, portMAX_DELAY);
}

wifi_config_t* wifi_app_get_wifi_config(void) {
    return wifi_config;
}

void wifi_app_start(void) {
	ESP_LOGI(TAG, "starting wifi application");

	rgb_led_app_started();

	esp_log_level_set("wifi", ESP_LOG_NONE);

    wifi_config = (wifi_config_t*) malloc(sizeof(wifi_config_t));
    memset(wifi_config, 0x00, sizeof(wifi_config_t));

	wifi_app_queue_handle = xQueueCreate(3, sizeof(wifi_app_queue_message_t));

	wifi_app_event_group = xEventGroupCreate();

	xTaskCreatePinnedToCore(
	        &wifi_app_task,
	        "wifi_app_task",
	        WIFI_APP_TASK_STACK_SIZE,
	        NULL,
	        WIFI_APP_TASK_PRIORITY,
	        NULL,
	        WIFI_APP_TASK_CORE_ID
	);
}
