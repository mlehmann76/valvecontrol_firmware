/*
 * gpioTask.c
 *
 *  Created on: 06.05.2018
 *      Author: marco
 */

#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "sdkconfig.h"
#include "esp_log.h"

#include "driver/gpio.h"

#include "gpioTask.h"
#include "config_user.h"

#define TAG "gpio_task"

static uint32_t actChan = 0;
static time_t lastStart = 0;

static void sendStatus(const queueData_t* pData);
static void disableChan(uint32_t chan);
static void enableChan(uint32_t chan);
static void updateGpio(void);
static int32_t bitToIndex(uint32_t bit);

static uint32_t gpioLut[] = {
		CONTROL0_PIN,
		CONTROL1_PIN,
		CONTROL2_PIN,
		CONTROL3_PIN};

void gpio_task(void *pvParameters) {
	while (1) {
		// timekeeping for auto off
		if (actChan != 0) {
			time_t now;
			time(&now);
			if (difftime(now, lastStart) > AUTO_OFF_SEC) {
				disableChan(actChan);
			}
		}
		if (subQueue != 0) {
			queueData_t rxData = { 0 };
			// Receive a message on the created queue.  Block for 10 ticks if a
			// message is not immediately available.
			if (xQueueReceive(subQueue, &(rxData), (TickType_t ) 10)) {
				ESP_LOGI(TAG, "received %08X, %d", rxData.chan, rxData.mode);

				switch (rxData.mode) {
				case mStatus:
					if (rxData.chan < (sizeof(gpioLut)/sizeof(gpioLut[0]))) {
						if ((actChan & (1<<rxData.chan)) != 0) {
							rxData.mode = mOn;
						} else {
							rxData.mode = mOff;
						}
					}
					sendStatus(&rxData);
					break;

				case mOn:
					enableChan(1<<rxData.chan);
					break;

				case mOff:
					disableChan(1<<rxData.chan);
					break;
				}
			}

			updateGpio();
		}
		vTaskDelay(10);
		taskYIELD();
	}
	vTaskDelete(NULL);
}

void gpio_task_setup(void) {
	gpio_config_t config = {.pin_bit_mask = (
			CONTROL0_BIT|CONTROL1_BIT|CONTROL2_BIT|CONTROL3_BIT),
			.mode = GPIO_MODE_OUTPUT};
	gpio_config(&config);
	for (int i=0; i< (sizeof(gpioLut)/sizeof(gpioLut[0]));i++) {
		gpio_set_level(gpioLut[i], 1);
	}
	xTaskCreate(&gpio_task, "gpio_task", 2048, NULL, 5, NULL);
}

static void sendStatus(const queueData_t* pData) {
	queueData_t data;
	data.chan = pData->chan;
	data.mode = pData->mode;

	if ( xQueueSendToBack(pubQueue, &data, 10) != pdPASS) {
		// Failed to post the message, even after 10 ticks.
		ESP_LOGI(TAG, "pubqueue post failure");
	} else {
		ESP_LOGI(TAG, "pubqueue post chan:%08X : %d -> %d", pData->chan, pData->mode, uxQueueMessagesWaiting(pubQueue));
	}
}

static void enableChan(uint32_t chan) {
	bool hasChanged = (actChan != 0) && (actChan != chan);
	if (hasChanged) {
		disableChan(actChan);
	}
	actChan = chan;
	updateGpio();
	queueData_t data = {bitToIndex(chan), mOn};
	if (hasChanged) sendStatus(&data);
	time(&lastStart);
}

static void disableChan(uint32_t chan) {
	if (actChan == chan) {
		queueData_t data = {bitToIndex(chan), mOff};
		sendStatus(&data);
		updateGpio();
		time(&lastStart);
		actChan = 0;
	}
}

static void updateGpio(void) {
	for (int i=0; i< (sizeof(gpioLut)/sizeof(gpioLut[0]));i++) {
		if ((actChan & (1<<i)) != 0) {
			gpio_set_level(gpioLut[i], 0);
		} else {
			gpio_set_level(gpioLut[i], 1);
		}
	}
}

static int32_t bitToIndex(uint32_t bit) {
	int32_t ret = -1;
	for (uint32_t i=0;i<32;i++) {
		if ((bit & (1<<i))!=0) {
			ret = i;
			break;
		}
	}
	return ret;
}
