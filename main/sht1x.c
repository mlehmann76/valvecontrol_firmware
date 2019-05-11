/*
 * sht1x.c
 *
 *  Created on: 09.04.2019
 *      Author: marco
 */

#include <stdio.h>
#include <time.h>

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

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
#define ACK_VAL 0x1                 /*!< I2C ack value */
#define NACK_VAL 0x0                /*!< I2C nack value */

typedef struct {
	SemaphoreHandle_t sem;
	float temp;
	float hum;
	bool hasError;
} sht1x_data_t;

static const char *TAG = "I2C";
static const int _us = 20;

static gpio_num_t i2c_gpio_sda = 21;
static gpio_num_t i2c_gpio_scl = 22;
//static uint32_t i2c_frequency = 100000;
//static i2c_port_t i2c_port = I2C_NUM_0;
static sht1x_data_t sensor_data = { NULL, 0, 0, false };
static void _sht1x_start(gpio_num_t i2c_gpio_sda, gpio_num_t i2c_gpio_scl) ;

static esp_err_t setupSHT1x(void) {
	return ESP_OK;
}

static void delay_usec(int64_t usec) {
	int64_t end = esp_timer_get_time() + usec;
	while (esp_timer_get_time() < end) {	};
//	for (int i=0;i<usec*100;i++) {;}
}

static void inline _sda_(gpio_num_t sda, int lvl) {
	if (lvl) {
		gpio_set_level(sda, 1);
		//gpio_set_direction(sda, GPIO_MODE_INPUT);
	} else {
		gpio_set_level(sda, 0);
		//gpio_set_direction(sda, GPIO_MODE_OUTPUT);
	}
}

static void inline _scl_(gpio_num_t scl, int lvl) {
	if (lvl) {
		gpio_set_level(scl, 1);
		 delay_usec(_us);
		//gpio_set_direction(scl, GPIO_MODE_INPUT);
	} else {
		gpio_set_level(scl, 0);
		 delay_usec(_us);
		//gpio_set_direction(scl, GPIO_MODE_OUTPUT);
	}
}


static void _sht1x_connectionreset(gpio_num_t i2c_gpio_sda, gpio_num_t i2c_gpio_scl) {
	_sda_(i2c_gpio_sda,1);
	_scl_(i2c_gpio_scl, 0);
	//reset
	for (int i = 0; i < 9; i++) {
		_scl_(i2c_gpio_scl, 1);
		_scl_(i2c_gpio_scl, 0);
	}
	_sht1x_start(i2c_gpio_sda, i2c_gpio_scl);
}

static void _sht1x_start(gpio_num_t i2c_gpio_sda, gpio_num_t i2c_gpio_scl) {
	//start pattern
	_sda_(i2c_gpio_sda, 1);
	_scl_(i2c_gpio_scl, 0);
	_scl_(i2c_gpio_scl, 1);
	_sda_(i2c_gpio_sda, 0); delay_usec(_us);
	_scl_(i2c_gpio_scl, 0);

	_scl_(i2c_gpio_scl, 1);
	_sda_(i2c_gpio_sda, 1); delay_usec(_us);
	_scl_(i2c_gpio_scl, 0);
}

static uint32_t _sht1x_read(gpio_num_t i2c_gpio_sda, gpio_num_t i2c_gpio_scl, uint32_t numBits, int ack) {
	uint32_t ret = 0;
	_sda_(i2c_gpio_sda,1);
	for (size_t i = 0; i < numBits; ++i) {
		_scl_(i2c_gpio_scl, 1);
		ret = ret * 2 + gpio_get_level(i2c_gpio_sda);
		_scl_(i2c_gpio_scl, 0);
	}

	_sda_(i2c_gpio_sda, ack == 0 ? 1 : 0);
	_scl_(i2c_gpio_scl, 1);
	_scl_(i2c_gpio_scl, 0);
	_sda_(i2c_gpio_sda, 1);

	ESP_LOGI(TAG, "read i2c %d", ret);
	return (ret);
}

static int _sht1x_write(gpio_num_t i2c_gpio_sda, gpio_num_t i2c_gpio_scl, uint32_t data) {
	int ack;
	for (size_t i = 0; i < 8; ++i) {
		_sda_(i2c_gpio_sda, (data & (1 << (7 - i))) ? 1 : 0);
		_scl_(i2c_gpio_scl, 1);
		_scl_(i2c_gpio_scl, 0);
	}
	_sda_(i2c_gpio_sda,1);
	_scl_(i2c_gpio_scl, 1);
	ack = gpio_get_level(i2c_gpio_sda);
	_scl_(i2c_gpio_scl, 0);
	return ack;
}

static void _sht_skip_crc(gpio_num_t i2c_gpio_sda, gpio_num_t i2c_gpio_scl) {
	// Skip acknowledge to end trans (no CRC)
	_sda_(i2c_gpio_sda,1);
	_scl_(i2c_gpio_scl, 1);
	_scl_(i2c_gpio_scl, 0);
}

static void _sht1x_reset(gpio_num_t i2c_gpio_sda, gpio_num_t i2c_gpio_scl) {
	_sht1x_connectionreset(i2c_gpio_sda, i2c_gpio_scl);
	_sht1x_write(i2c_gpio_sda, i2c_gpio_scl, SHT1X_RESET);
}

static esp_err_t readSHT1xReg16(uint8_t reg, uint16_t *pData) {

	esp_err_t ret = ESP_OK;
	int ack = 0;
	_sht1x_start(i2c_gpio_sda, i2c_gpio_scl);
	ack += _sht1x_write(i2c_gpio_sda, i2c_gpio_scl, reg);

	if (ack != 0) {
		ESP_LOGI(TAG, "error on ack i2c 1");
		ret = ESP_ERR_TIMEOUT;
	}

	time_t start,now;
	time(&now);
	time(&start);

	while (1) {
		time(&now);
		if (gpio_get_level(i2c_gpio_sda) == 0) {
			break;
		}
		if (difftime(now, start) >= 1) {
			ESP_LOGI(TAG, "timeout on waiting i2c ");
			ret = ESP_ERR_TIMEOUT;
			break;
		}
	}

	uint16_t val = _sht1x_read(i2c_gpio_sda, i2c_gpio_scl, 8, ACK_VAL);
	val *= 256;

	val |= _sht1x_read(i2c_gpio_sda, i2c_gpio_scl, 8, ACK_VAL);
	uint8_t c = _sht1x_read(i2c_gpio_sda, i2c_gpio_scl, 8, NACK_VAL);

	*pData = val;

	_scl_(i2c_gpio_scl, 1);
	_sda_(i2c_gpio_sda,1);
	return ret;
}

static float readSHT1xTemp(void) {
	static const float d1 = -40.1;
	static const float d2 = 0.01; //12bit

	uint16_t temp = 0;
	if (readSHT1xReg16(SHT1X_MEASURE_TEMP, &temp) != ESP_OK) {
		temp = 0;
		sensor_data.hasError = true;
	} else {
		sensor_data.hasError = false;
	}

	temp &= 0x3FFF;

	return (d1 + d2 * temp);
}

static float readSHT1xHum(void) {
	static const float c1 = -4.0;
	static const float c2 = +0.0405;
	static const float c3 = -0.0000028;

	uint16_t temp = 0;
	if (readSHT1xReg16(SHT1X_MEASURE_HUM, &temp) != ESP_OK) {
		temp = 0;
		sensor_data.hasError = true;
	} else {
		sensor_data.hasError = false;
	}

	temp &= 0x0FFF;

	return (c1 + c2 * temp + c3 * temp * temp);
}

void addSHT1xStatus(cJSON *root) {

	char buf[16];
	const TickType_t xTicksToWait = 10000 / portTICK_PERIOD_MS;

	if ((sensor_data.sem != NULL) && xSemaphoreTake( sensor_data.sem, xTicksToWait) == pdTRUE) {

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

		snprintf(buf, sizeof(buf), "%3.2f", sensor_data.temp);
		if (cJSON_AddStringToObject(channelv, "temp", buf) == NULL) {
			goto end;
		}

		snprintf(buf, sizeof(buf), "%3.2f", sensor_data.hum);
		if (cJSON_AddStringToObject(channelv, "hum", buf) == NULL) {
			goto end;
		}

		if (cJSON_AddStringToObject(channelv, "status", sensor_data.hasError ? "err" : "ok") == NULL) {
			goto end;
		}

		end:
		xSemaphoreGive(sensor_data.sem);
	}
	return;
}

void sht1x_task(void *pvParameters) {
	while (1) {
		if (1) {
			//block till data was processed by ota task
			const TickType_t xTicksToWait = 10000 / portTICK_PERIOD_MS;
			if ((sensor_data.sem != NULL) && xSemaphoreTake( sensor_data.sem, xTicksToWait) == pdTRUE) {

				_sht1x_reset(i2c_gpio_sda, i2c_gpio_scl);
				vTaskDelay(20 / portTICK_PERIOD_MS);

				sensor_data.temp = readSHT1xTemp();
				sensor_data.hum = readSHT1xHum();

				if (status_event_group != NULL) {
					xEventGroupSetBits(status_event_group, STATUS_EVENT_SENSOR);
				}
				xSemaphoreGive(sensor_data.sem);
			}
			vTaskDelay(60000 / portTICK_PERIOD_MS);
		} else {
			vTaskDelay(10000 / portTICK_PERIOD_MS);
		}
	}
	vTaskDelete(NULL);
}

void setupSHT1xTask(void) {
	esp_err_t err = ESP_OK;
	//err = i2c_master_driver_initialize();
	gpio_config_t io_conf;
	io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
	io_conf.mode = GPIO_MODE_INPUT_OUTPUT_OD;
	io_conf.pin_bit_mask = (1 << i2c_gpio_sda) | (1 << i2c_gpio_scl);
	io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
	gpio_config(&io_conf);
	gpio_set_level(i2c_gpio_scl, 1);
	gpio_set_level(i2c_gpio_sda, 1);

	if (err == ESP_OK) {
		//err = i2c_driver_install(i2c_port, I2C_MODE_MASTER, 0, 0, 0);

		if (err == ESP_OK) {

			sensor_data.sem = xSemaphoreCreateBinary();

			if (sensor_data.sem == NULL) {
				ESP_LOGE(TAG, "error creating semaphore");
			} else {
				xSemaphoreGive(sensor_data.sem);
			}

			setupSHT1x();

			xTaskCreate(&sht1x_task, "sht1x_task", 2048, NULL, 5, NULL);
		}
	}
}
