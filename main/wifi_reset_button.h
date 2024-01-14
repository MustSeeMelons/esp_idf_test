/*
 * wifi_reset_button.h
 *
 *  Created on: 2024. gada 14. janv.
 *      Author: Strau
 */

#ifndef MAIN_WIFI_RESET_BUTTON_H_
#define MAIN_WIFI_RESET_BUTTON_H_

#define ESP_INTR_FLAG_DEFAULT 0

// Reset button is the BOOT button on the dev kit
#define WIFI_RESET_BUTTON 0

void wifi_reset_button_config(void);

#endif /* MAIN_WIFI_RESET_BUTTON_H_ */
