/*
 * rgb_led.h
 *
 *  Created on: 2024. gada 1. janv.
 *      Author: Strau
 */

#ifndef MAIN_RGB_LED_H_
#define MAIN_RGB_LED_H_

// RGB LED GPIOs
#define RGB_LED_RED_GPIO 33
#define RGB_LED_GREEN_GPIO 32
#define RGB_LED_BLUE_GPIO 23

// RGB LED colour mix channels
#define RGB_LED_CHANNEL_NUM 3

// RGB LED configuration
typedef struct {
	int channel;
	int gpio;
	int mode;
	int timer_index;
} ledc_info_t;

// Colour for WiFi App started.
void rgb_led_app_started(void);

// HTTP server has started
void rgb_led_http_started(void);

// Connected to AP
void rgb_led_wifi_connected(void);

#endif /* MAIN_RGB_LED_H_ */
