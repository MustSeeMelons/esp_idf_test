/*
 * AM2320.h
 *
 *  Created on: 2024. gada 4. janv.
 *      Author: Strautins
 */

#ifndef MAIN_AM2320_H_
#define MAIN_AM2320_H_

#include "driver/i2c.h"

void am2320_task_start(void);

void am2320_init(void);

/**
 * We must wake the sensor before every read.
 */
esp_err_t am2320_wake(void);

/**
 * After the device has been woken up, must send a read command to specify what we want.
 */
esp_err_t am2320_send_read(void);

/**
 * Once device has woken up and we have told what we want - we can finally perform the read.
 */
esp_err_t am2320_read_data(void);

/**
 * Performs all the fun stuff to get sensor readings.
 */
void am2320_fetch_measurements(void);

float get_temprature();

float get_humidity();

#endif /* MAIN_AM2320_H_ */
