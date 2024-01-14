/*
 * AM2320.c
 *
 *  Created on: 2024. gada 4. janv.
 *      Author: Strautins
 */
#include "AM2320.h"
#include "esp_log.h"
#include "rom/ets_sys.h"

#include "tasks_common.h"

static const char* TAG = "AM2320";

static float _temptrature = 18.2;
static float _humidity = 39.9;

const uint8_t I2C_PORT = I2C_NUM_0;

// 0xB8 is an 8 bit number, we can | with write/read and be golden
// 0x5C is an 7 but number, we must << 1 before we or with operation
const uint8_t addr = 0xB8;

void am2320_init(void) {
    ESP_LOGI(TAG, "Initializing I2C");

    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 21,
        .scl_io_num = 22,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = 5000, // 5 kHz
    };

    esp_err_t config_result = i2c_param_config(I2C_PORT, &i2c_conf);

    if (config_result != ESP_OK) {
        ESP_LOGI(TAG, "Failed to create I2C config!");
        return;
    }

    esp_err_t driver_result = i2c_driver_install(I2C_PORT, i2c_conf.mode, 0, 0, 0);

    if (driver_result != ESP_OK) {
        ESP_LOGI(TAG, "Failed to install I2C driver!");
        return;
    }

    i2c_set_timeout(I2C_PORT, 0xFFFFF);

    ESP_LOGI(TAG, "I2C OK!");
}

esp_err_t am2320_wake(void) {
    esp_err_t err = ESP_OK;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    err = i2c_master_start(cmd);
    if (err != ESP_OK) {
        i2c_cmd_link_delete(cmd);
        return err;
    }

    // Write address, don't wait for acknowledge
    err = i2c_master_write_byte(cmd, addr | I2C_MASTER_WRITE, false);
    if (err != ESP_OK) {
        i2c_cmd_link_delete(cmd);
        return err;
    }

    err = i2c_master_stop(cmd);
    if (err != ESP_OK) {
        i2c_cmd_link_delete(cmd);
        return err;
    }

    err = i2c_master_cmd_begin(I2C_PORT, cmd, 2 / portTICK_PERIOD_MS);
    if (err != ESP_OK) {
        i2c_cmd_link_delete(cmd);
        return err;
    }

    i2c_cmd_link_delete(cmd);

    return err;
}

esp_err_t am2320_send_read(void) {
    esp_err_t err = ESP_OK;

    // Function code, start address, register length
    const uint8_t write_buff[3] = { 0x03, 0x00, 0x04 };

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    err = i2c_master_start(cmd);
    if (err != ESP_OK) {
        i2c_cmd_link_delete(cmd);
        return err;
    }

    err = i2c_master_write_byte(cmd, addr | I2C_MASTER_WRITE, true);
    if (err != ESP_OK) {
        i2c_cmd_link_delete(cmd);
        return err;
    }

    err = i2c_master_write(cmd, write_buff, 3, true);
    if (err != ESP_OK) {
        i2c_cmd_link_delete(cmd);
        return err;
    }

    err = i2c_master_stop(cmd);
    if (err != ESP_OK) {
        i2c_cmd_link_delete(cmd);
        return err;
    }

    err = i2c_master_cmd_begin(I2C_PORT, cmd, 1000 / portTICK_PERIOD_MS);

    i2c_cmd_link_delete(cmd);

    return err;
}

/**
 * Performs a CRC-16-CCITT on the buffer to a certain length & returs true if they match.
 */
static bool am2320_is_crc_match(uint8_t *data, unsigned char len, uint16_t crc) {
    uint16_t calcCrc = 0xFFFF;

    while (len--) {
        calcCrc ^= *data++;
        for (int i = 0; i < 8; i++) {
            if (calcCrc & 0x01) {
                calcCrc >>= 1;
                calcCrc ^= 0xA001;
            } else {
                calcCrc >>= 1;
            }
        }
    }

    return calcCrc == crc;
}

esp_err_t am2320_read_data(void) {
    esp_err_t err = ESP_OK;

    const uint8_t byte_count = 8;
    uint8_t data[byte_count];

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    err = i2c_master_start(cmd);
    if (err != ESP_OK) {
        i2c_cmd_link_delete(cmd);
        return err;
    }

    err = i2c_master_write_byte(cmd, addr | I2C_MASTER_READ, true);
    if (err != ESP_OK) {
        i2c_cmd_link_delete(cmd);
        return err;
    }

    err = i2c_master_read(cmd, data, byte_count, I2C_MASTER_LAST_NACK);
    if (err != ESP_OK) {
        i2c_cmd_link_delete(cmd);
        return err;
    }

    err = i2c_master_stop(cmd);
    if (err != ESP_OK) {
        i2c_cmd_link_delete(cmd);
        return err;
    }

    err = i2c_master_cmd_begin(I2C_PORT, cmd, 1000 / portTICK_PERIOD_MS);

    i2c_cmd_link_delete(cmd);

    if (err != ESP_OK) {
        return err;
    } else {
        uint16_t crc = (data[7] << 8) | data[6];

        bool is_match = am2320_is_crc_match(data, byte_count - 2, crc);

        if(is_match) {
            float humidity = (float)((data[2] << 8) | data[3]) / 10;
            _humidity = humidity;

            bool isNegative = data[4] & (1 << 8);
            if (isNegative) {
                data[4] = data[4] | (1 << 8);
            }

            float temprature = (float)((data[4] << 8) | data[5]) / 10;
            _temptrature = temprature;

//            ESP_LOGI(TAG, "Code:       %d", data[0]);
//            ESP_LOGI(TAG, "Length      %d", data[1]);
//            ESP_LOGI(TAG, "humidity:   %.2f", humidity);
//            ESP_LOGI(TAG, "temprature: %.2f", temprature);
//            ESP_LOGI(TAG, "crc:        %d", crc);
        } else {
            ESP_LOGI(TAG, "CRC Mismatch.");
        }
    }

    return err;
}

/**
 * Looks up error & outputs it.
 */
static void am2320_handle_error(esp_err_t err) {
    const char *msg = esp_err_to_name(err);
    ESP_LOGI(TAG, "Ooops: %s", msg);
}

void am2320_fetch_measurements(void) {
    esp_err_t err = ESP_OK;


    err = am2320_wake();

    if (err != ESP_OK) {
        am2320_handle_error(err);
        return;
    }

    err = am2320_send_read();

    if (err != ESP_OK) {
        am2320_handle_error(err);
        return;
    }

    // Need to wait a bit, 1.5+ms
    ets_delay_us(2000);

    err = am2320_read_data();
    if (err != ESP_OK) {
        am2320_handle_error(err);
        return;
    }
}

float get_temprature() {
    return _temptrature;
}

float get_humidity() {
    return _humidity;
}

static void am2320_task(void *pvParameter) {
    printf("Starting am2320 task\n\n");

    for(;;) {
        am2320_fetch_measurements();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void am2320_task_start(void) {
    xTaskCreatePinnedToCore(
            &am2320_task,
            "AM2320",
            AM2320_TASK_STACK_SIZE,
            NULL,
            AM2320_TASK_PRIORITY,
            NULL,
            AM2320_TASK_CORE_ID
    );
}
