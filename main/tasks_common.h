/*
 * tasks_common.h
 *
 *  Created on: 2024. gada 2. janv.
 *      Author: Strautins
 */

#ifndef MAIN_TASKS_COMMON_H_
#define MAIN_TASKS_COMMON_H_

#define WIFI_APP_TASK_STACK_SIZE 	        4096
#define WIFI_APP_TASK_PRIORITY 		        5
#define WIFI_APP_TASK_CORE_ID 		        0

#define HTTP_SERVER_STACK_SIZE              8192
#define HTTP_SERVER_TASK_PRIORITY           4
#define HTTP_SERVER_TASK_CORE_ID            0

#define HTTP_SERVER_MONITOR_STACK_SIZE      4096
#define HTTP_SERVER_MONITOR_PRIORITY        3
#define HTTP_SERVER_MONITOR_CORE_ID         0

#define AM2320_TASK_STACK_SIZE              4096
#define AM2320_TASK_PRIORITY                5
#define AM2320_TASK_CORE_ID                 1

#define WIFI_RESET_BUTTON_TASK_STACK_SIZE   2048
#define WIFI_RESET_BUTTON_TASK_PRIORITY     6
#define WIFI_RESET_BUTTON_TASK_CORE_ID      0

#endif /* MAIN_TASKS_COMMON_H_ */
