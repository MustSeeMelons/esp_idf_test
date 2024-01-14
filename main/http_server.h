/*
 * http_server.h
 *
 *  Created on: 2024. gada 3. janv.
 *      Author: Strau
 */

#ifndef MAIN_HTTP_SERVER_H_
#define MAIN_HTTP_SERVER_H_

#define OTA_UPDATE_PENDING 0
#define OTA_UPDATE_SUCCESSFUL 1
#define OTA_UPDATE_FAILED -1

typedef enum http_server_wifi_connect_status {
    NONE = 0,
    HTTP_WIFI_STATUS_CONNECTING,
    HTTP_WIFI_STATUS_CONNECT_FAILED,
    HTTP_WIFI_STATUS_CONNECT_SUCCESS,
    HTTP_WIFI_STATUS_DISCONNECTED
} http_server_wifi_connect_status;

typedef enum http_server_message {
    HTTP_MSG_WIFI_CONNECT_INIT = 0,
    HTTP_MSG_WIFI_CONNEC_SUCCESS,
    HTTP_MSG_WIFI_CONNECT_FAIL,
    HTTP_MSG_USER_DISCONNECT,
    HTTP_MSG_OTA_UPDATE_SUCCESSFUL,
    HTTP_MSG_OTA_UPDATE_FAILED
} http_server_message_e;

typedef struct http_server_queue_message {
        http_server_message_e msgID;
} http_server_queue_message_t;

BaseType_t http_server_monitor_send_message(http_server_message_e msgID);

void http_server_start(void);

void http_server_stop(void);

void http_server_fw_update_reset_callback(void *arg);

#endif /* MAIN_HTTP_SERVER_H_ */
