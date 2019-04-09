/*
 * sht1x.c
 *
 *  Created on: 09.04.2019
 *      Author: marco
 */

#include <stdio.h>

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "driver/i2c.h"
#include "esp_log.h"

#include "cJSON.h"

#include "sht1x.h"
#include "status.h"

#define I2C_MASTER_TX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define WRITE_BIT I2C_MASTER_WRITE  /*!< I2C master write */
#define READ_BIT I2C_MASTER_READ    /*!< I2C master read */
#define ACK_CHECK_EN 0x1            /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0           /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0                 /*!< I2C ack value */
#define NACK_VAL 0x1                /*!< I2C nack value */

static const char *TAG = "I2C";

static gpio_num_t i2c_gpio_sda = 21;
static gpio_num_t i2c_gpio_scl = 22;
static uint32_t i2c_frequency = 100000;
static i2c_port_t i2c_port = I2C_NUM_0;
static bool hasError = false;

static esp_err_t i2c_master_driver_initialize()
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = i2c_gpio_sda,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = i2c_gpio_scl,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = i2c_frequency
    };
    return i2c_param_config(i2c_port, &conf);
}

static esp_err_t setupSHT1x(void) {
	return ESP_OK;
}

static esp_err_t readSHT1xReg16(uint8_t reg, uint16_t *pData) {
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);

	i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
	esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, 1000 / portTICK_RATE_MS);
	if (ret != ESP_OK) { ESP_LOGI(TAG, "error on reading i2c : %s", esp_err_to_name(ret));}

	vTaskDelay(SHT1X_MEASURE_TIME / portTICK_PERIOD_MS);

	i2c_master_read(cmd, (uint8_t*)pData, sizeof(*pData), ACK_VAL);
	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(i2c_port, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);

	if (ret != ESP_OK) { ESP_LOGI(TAG, "error on reading i2c : %s", esp_err_to_name(ret));}

	return ret;
}

static float readSHT1xTemp(void) {
	static const float d1 = -40.1;
	static const float d2 = 0.04; //12bit

	uint16_t temp = 0;
	if (readSHT1xReg16(SHT1X_MEASURE_TEMP, &temp) != ESP_OK) {
		temp = 0;
		hasError = true;
	} else {
		hasError = false;
	}

	return (d1 + d2 * temp);
}

static float readSHT1xHum(void) {
	static const float c1 = -2.0468;
	static const float c2 = 0.5872;
	static const float c3 = -4.0845E-4;

	uint16_t temp = 0;
	if (readSHT1xReg16(SHT1X_MEASURE_HUM, &temp) != ESP_OK) {
		temp = 0;
		hasError = true;
	} else {
		hasError = false;
	}

	return (c1 + c2 * temp + c3 * temp *temp);
}

void addSHT1xStatus(cJSON *root) {

	char buf[16];

	if (root == NULL) {
		goto end;
	}

	cJSON *channel = cJSON_AddObjectToObject(root, "sensors");
	if (channel == NULL) {
		goto end;
	}

	cJSON *channelv = cJSON_AddObjectToObject(channel, "sht1x");
	if (channelv == NULL) {
		goto end;
	}

	snprintf(buf, sizeof(buf), "%3.2f", readSHT1xTemp());
	if (cJSON_AddStringToObject(channelv, "temp", buf) == NULL) {
		goto end;
	}

	snprintf(buf, sizeof(buf), "%3.2f", readSHT1xHum());
	if (cJSON_AddStringToObject(channelv, "hum", buf) == NULL) {
		goto end;
	}


	end:

	return;
}

void sht1x_task(void *pvParameters) {

	while (1) {
		if (!hasError) {
			if (status_event_group != NULL) {
				xEventGroupSetBits(status_event_group, STATUS_EVENT_SENSOR);
			}
			vTaskDelay(60000 / portTICK_PERIOD_MS);
		} else {
			vTaskDelay(10000 / portTICK_PERIOD_MS);
		}
	}
	vTaskDelete(NULL);
}

void setupSHT1xTask(void) {
	i2c_master_driver_initialize();
	setupSHT1x();

	xTaskCreate(&sht1x_task, "sht1x_task", 2048, NULL, 5, NULL);
}
