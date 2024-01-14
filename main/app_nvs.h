/*
 * app_nvs.h
 *
 *  Created on: 2024. gada 11. janv.
 *      Author: Strau
 */



#ifndef MAIN_APP_NVS_H_
#define MAIN_APP_NVS_H_

esp_err_t app_nvs_save_sta_creds(void);

bool app_nvs_load_sta_creds(void);

esp_err_t app_nvs_clear_sta_creds(void);

#endif /* MAIN_APP_NVS_H_ */
