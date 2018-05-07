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

#include "gpioTask.h"

#define TAG "gpio_task"

static uint32_t actChan = 0;
static time_t lastStart = 0;

static void sendStatus(const queueData_t* pData);
static void disableChan(uint32_t chan);
static void enableChan(uint32_t chan);

void gpio_task(void *pvParameters) {
	while (1) {
		// timekeeping for auto off
		if (actChan != 0) {
			time_t now;
			time(&now);
			if (difftime(now, lastStart) > 5) {
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
					sendStatus(&rxData);
					break;

				case mOn:
					enableChan(rxData.chan);
					break;

				case mOff:
					disableChan(rxData.chan);
					break;
				}

			}
		}
		taskYIELD();
	}
	vTaskDelete(NULL);
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
	queueData_t data = {chan, mOn};
	if (hasChanged) sendStatus(&data);
	time(&lastStart);
}

static void disableChan(uint32_t chan) {
	if (actChan == chan) {
		queueData_t data = {chan, mOff};
		sendStatus(&data);
	}
	actChan = 0;
	time(&lastStart);
}
