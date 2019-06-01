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

sht1x_handle_t *psht1x_handle = NULL;


static const char *TAG = "I2C";
static const int _us = 20;

static esp_err_t setupSHT1x(void) {
	return ESP_OK;
}

static void delay_usec(int64_t usec) {
	int64_t end = esp_timer_get_time() + usec;
	while (esp_timer_get_time() < end) {	};
}

static void inline _sda_(const sht1x_handle_t* pHandle, int lvl) {
	//ESP_LOGI(TAG, "sda gpio %d", pHandle->i2c_gpio_sda);
	gpio_set_level(pHandle->i2c_gpio_sda, lvl ? 1 : 0);
}

static void inline _scl_(const sht1x_handle_t* pHandle, int lvl) {
	//ESP_LOGI(TAG, "scl gpio %d", pHandle->i2c_gpio_scl);
	gpio_set_level(pHandle->i2c_gpio_scl, lvl ? 1 : 0);
	delay_usec(_us);
}

static void _sht1x_start(const sht1x_handle_t* pHandle) {
	//start pattern
	_sda_(pHandle, 1);
	_scl_(pHandle, 0);
	_scl_(pHandle, 1);
	_sda_(pHandle, 0); delay_usec(_us);
	_scl_(pHandle, 0);

	_scl_(pHandle, 1);
	_sda_(pHandle, 1); delay_usec(_us);
	_scl_(pHandle, 0);
}

static void _sht1x_connectionreset(const sht1x_handle_t* pHandle) {
	_sda_(pHandle, 1);
	_scl_(pHandle, 0);
	//reset
	for (int i = 0; i < 9; i++) {
		_scl_(pHandle, 1);
		_scl_(pHandle, 0);
	}
	_sht1x_start(pHandle);
}

static uint32_t _sht1x_read(const sht1x_handle_t* pHandle, uint32_t numBits, int ack) {
	uint32_t ret = 0;
	_sda_(pHandle,1);
	for (size_t i = 0; i < numBits; ++i) {
		_scl_(pHandle, 1);
		ret = ret * 2 + gpio_get_level(pHandle->i2c_gpio_sda);
		_scl_(pHandle, 0);
	}

	_sda_(pHandle, ack == 0 ? 1 : 0);
	_scl_(pHandle, 1);
	_scl_(pHandle, 0);
	_sda_(pHandle, 1);

	ESP_LOGI(TAG, "read i2c %d", ret);
	return (ret);
}

static int _sht1x_write(const sht1x_handle_t* pHandle, uint32_t data) {
	int ack;
	for (size_t i = 0; i < 8; ++i) {
		_sda_(pHandle, (data & (1 << (7 - i))) ? 1 : 0);
		_scl_(pHandle, 1);
		_scl_(pHandle, 0);
	}
	_sda_(pHandle,1);
	_scl_(pHandle, 1);
	ack = gpio_get_level(pHandle->i2c_gpio_sda);
	_scl_(pHandle, 0);
	return ack;
}

static void _sht1x_reset(const sht1x_handle_t* pHandle) {
	_sht1x_connectionreset(pHandle);
	_sht1x_write(pHandle, SHT1X_RESET);
}

static esp_err_t readSHT1xReg16(const sht1x_handle_t* pHandle, uint8_t reg, uint16_t *pData) {

	esp_err_t ret = ESP_OK;
	int ack = 0;
	_sht1x_start(pHandle);
	ack += _sht1x_write(pHandle, reg);

	if (ack != 0) {
		ESP_LOGI(TAG, "error on ack i2c 1");
		ret = ESP_ERR_TIMEOUT;
	}

	time_t start,now;
	time(&now);
	time(&start);

	while (1) {
		time(&now);
		if (gpio_get_level(pHandle->i2c_gpio_sda) == 0) {
			break;
		}
		if (difftime(now, start) >= 2) {
			ESP_LOGI(TAG, "timeout on waiting i2c ");
			ret = ESP_ERR_TIMEOUT;
			break;
		}
	}

	uint16_t val = _sht1x_read(pHandle, 8, ACK_VAL);
	val *= 256;
	val |= _sht1x_read(pHandle, 8, ACK_VAL);

	_sht1x_read(pHandle, 8, NACK_VAL);

	*pData = val;

	_sda_(pHandle, 1);
	_scl_(pHandle, 1);
	return ret;
}

static void readSHT1xTemp(sht1x_handle_t* pHandle) {
	static const float d1 = -40.1;
	static const float d2 = 0.01; //12bit

	uint16_t soth = 0;
	if (readSHT1xReg16(pHandle, SHT1X_MEASURE_TEMP, &soth) != ESP_OK) {
		soth = 0;
		pHandle->hasError = true;
	} else {
		pHandle->hasError = false;
	}

	soth &= 0x3FFF;

	pHandle->temp = (d1 + d2 * soth);
}

static void readSHT1xHum(sht1x_handle_t* pHandle) {
	static const float c1 = -2.0468;
	static const float c2 = +0.0367;
	static const float c3 = -1.5955E-6;

	uint16_t sorh = 0;
	if (readSHT1xReg16(pHandle, SHT1X_MEASURE_HUM, &sorh) != ESP_OK) {
		sorh = 0;
		pHandle->hasError = true;
	} else {
		pHandle->hasError = false;
	}

	sorh &= 0x0FFF;

	float rth = (c1 + c2 * sorh + c3 * sorh * sorh);

	/* disabled for testing
	// temperature compensation
	if (!pHandle->hasError) {
		rth = (pHandle->temp - 25) * (0.01 + 0.00008 * sorh) + rth;
	}
	*/
	pHandle->hum = rth;
}

void addSHT1xStatus(cJSON *root) {

	char buf[16];
	const TickType_t xTicksToWait = 10000 / portTICK_PERIOD_MS;

	if ((psht1x_handle != NULL) && (psht1x_handle->sem != NULL) && xSemaphoreTake( psht1x_handle->sem, xTicksToWait) == pdTRUE) {

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

		snprintf(buf, sizeof(buf), "%3.2f", psht1x_handle->temp);
		if (cJSON_AddStringToObject(channelv, "temp", buf) == NULL) {
			goto end;
		}

		snprintf(buf, sizeof(buf), "%3.2f", psht1x_handle->hum);
		if (cJSON_AddStringToObject(channelv, "hum", buf) == NULL) {
			goto end;
		}

		if (cJSON_AddStringToObject(channelv, "status", psht1x_handle->hasError ? "err" : "ok") == NULL) {
			goto end;
		}

		end:
		xSemaphoreGive(psht1x_handle->sem);
	}
	return;
}

void sht1x_task(void *pvParameters) {
	sht1x_handle_t* pHandle = (sht1x_handle_t*) pvParameters;
	while (1) {
		if (1) {
			//block till data was processed by ota task
			const TickType_t xTicksToWait = 10000 / portTICK_PERIOD_MS;
			if ((pHandle->sem != NULL) && xSemaphoreTake( pHandle->sem, xTicksToWait) == pdTRUE) {

				if (pHandle->hasError) {
					_sht1x_reset(pHandle);
					pHandle->hasError = false;
					vTaskDelay(20 / portTICK_PERIOD_MS);
				}

				readSHT1xTemp(pHandle);
				readSHT1xHum(pHandle);

				if (status_event_group != NULL) {
					xEventGroupSetBits(status_event_group, STATUS_EVENT_SENSOR);
				}
				xSemaphoreGive(pHandle->sem);
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
	static sht1x_handle_t sht1x_handle;
	sht1x_handle.i2c_gpio_sda = GPIO_NUM_21;
	sht1x_handle.i2c_gpio_scl = GPIO_NUM_22;

	gpio_config_t io_conf;
	io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
	io_conf.mode = GPIO_MODE_INPUT_OUTPUT_OD;
	io_conf.pin_bit_mask = (1 << sht1x_handle.i2c_gpio_sda) | (1 << sht1x_handle.i2c_gpio_scl);
	io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	io_conf.pull_up_en = GPIO_PULLUP_ENABLE;

	gpio_config(&io_conf);
	gpio_set_level(sht1x_handle.i2c_gpio_scl, 1);
	gpio_set_level(sht1x_handle.i2c_gpio_sda, 1);

	if (err == ESP_OK) {

		sht1x_handle.sem = xSemaphoreCreateBinary();

		if (sht1x_handle.sem == NULL) {
			ESP_LOGE(TAG, "error creating semaphore");
		} else {
			xSemaphoreGive(sht1x_handle.sem);
		}

		setupSHT1x();
		psht1x_handle = &sht1x_handle;

		xTaskCreate(&sht1x_task, "sht1x_task", 2048, &sht1x_handle, 5, NULL);
	}
}
