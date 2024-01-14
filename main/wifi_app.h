/*
 * wifi_app.h
 *
 *  Created on: 2024. gada 2. janv.
 *      Author: Strau
 */

#ifndef MAIN_WIFI_APP_H_
#define MAIN_WIFI_APP_H_

#include "esp_netif.h"
#include "esp_wifi.h"

#define WIFI_AP_SSID 				"ESP32_AP"
#define WIFI_AP_PASSWORD 			"password"
#define WIFI_AP_CHANNEL 			1
#define WIFI_AP_SSID_HIDDEN			0
#define WIFI_AP_MAX_CONNECTIONS 	5
#define WIFI_AP_BEACON_INTERVAL 	100 // ms, as recommended
#define WIFI_AP_IP					"192.168.0.1"
#define WIFI_AP_GATEWAY				"192.168.0.1"
#define WIFI_AP_NETMASK				"255.255.255.0"
#define WIFI_AP_BANDWIDTH			WIFI_BW_HT20 // or 40, 20 less speed less interference
#define WIFI_AP_POWER_SAVE			WIFI_PS_NONE
#define WIFI_AP_SSID_LENGTH			32 // IEEE standard max
#define WIFI_AP_PASSWORD_LENGTH		64 // IEEE standard max
#define WIFI_AP_CONNECTION_RETRIES	5 // on disconnect

// netif objects for station and AP
extern esp_netif_t *esp_netif_sta;
extern esp_netif_t *esp_netif_ap;

// Message ID's for the WiFi task
typedef enum wifi_app_message {
	WIFI_APP_MSG_START_HTTP_SERVER = 0,
	WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER,
	WIFI_APP_MSG_STA_CONNECTED_GOT_IP,
	WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT,
	WIFI_APP_MSG_LOAD_SAVED_CREDS,
	WIFI_APP_MSG_STA_DISCONNECTED
} wifi_app_message_e;

// Message q structure
typedef struct wifi_app_queue_message {
	wifi_app_message_e msgID;
} wifi_app_queue_message_t;

// Send message to the queue
BaseType_t wifi_app_send_message(wifi_app_message_e msgID);

void wifi_app_start(void);

wifi_config_t* wifi_app_get_wifi_config(void);

#endif /* MAIN_WIFI_APP_H_ */
