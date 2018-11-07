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
#include "freertos/event_groups.h"

#include "sdkconfig.h"
#include "esp_log.h"
#include "cJSON.h"

#include "mqtt_config.h"
#include "mqtt_client.h"
#include "mqtt_user.h"

#include "driver/gpio.h"

#include "controlTask.h"
#include "config_user.h"

#define TAG "gpio_task"


messageHandler_t controlHandler = {.pUserctx = NULL, .onMessage = handleControlMsg};


static uint32_t actChan = 0;
static time_t lastStart = 0;
static bool isConnected = false;

static void handleChannelControl(cJSON* chan);
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

static int checkButton() {
	static int wps_button_count = 0;
	if ((gpio_get_level(WPS_BUTTON) == 0)) {
		wps_button_count++;
		if (wps_button_count > (WPS_LONG_MS / portTICK_PERIOD_MS)) {
			xEventGroupSetBits(button_event_group, WPS_LONG_BIT);
			wps_button_count = 0;
		}
	} else {
		if (wps_button_count > (WPS_SHORT_MS / portTICK_PERIOD_MS)) {
			xEventGroupSetBits(button_event_group, WPS_SHORT_BIT);
		}
		wps_button_count = 0;
	}
	return wps_button_count;
}

int handleControlMsg(pCtx_t ctx, esp_mqtt_event_handle_t event) {
	int ret = 0;
	if (event->topic_len > strlen(getSubMsg())) {
		const char* pTopic = &event->topic[strlen(getSubMsg()) - 1];
		//check for control messages
		if (strcmp(pTopic, "control") == 0) {
			ESP_LOGI(TAG, "%.*s", event->topic_len - strlen(getSubMsg()) + 1,
					pTopic);
			cJSON *root = cJSON_Parse(event->data);
			cJSON *chan = cJSON_GetObjectItem(root, "channel");
			if (chan != NULL) {
				handleChannelControl(chan);
			}
			cJSON_Delete(root);
			ret = 1;
		}
	}
	return ret;
}

/**
 * control format for channel control
 * {
 * 		"channel":
 * 		{
 * 			"num": 1,
 * 			"val": 1
 * 		}
 * }
 */
static void handleChannelControl(cJSON* chan) {
	if (cJSON_GetObjectItem(chan, "num") != NULL) {
		int chanNum = cJSON_GetObjectItem(chan, "num")->valueint;
		int chanVal = -1;
		if (cJSON_GetObjectItem(chan, "val") != NULL) {
			chanVal = cJSON_GetObjectItem(chan, "val")->valueint;
		}

		ESP_LOGD(TAG, "channel :%d found ->%d", chanNum, chanVal);
		gpio_task_mode_t func = mStatus;

		if (chanVal == 1) {
			func = mOn;
		} else if (chanVal == 0) {
			func = mOff;
		} else {
			func = mStatus;
		}

		queueData_t cdata = { chanNum, func };
		const TickType_t tick = 10 / portTICK_PERIOD_MS;
		// available if necessary.
		if (xQueueSend(subQueue, (void * ) &cdata,	tick) != pdPASS) {
			// Failed to post the message, even after 10 ticks.
			ESP_LOGW(TAG, "subqueue post failure");
		}

	}
}

void gpio_task(void *pvParameters) {

	while (1) {

		if (!isConnected && (xEventGroupGetBits(mqtt_event_group) & MQTT_CONNECTED_BIT)) {
			isConnected = true;
			gpio_onConnect();
		}

		if (!(xEventGroupGetBits(mqtt_event_group) & MQTT_CONNECTED_BIT)) {
			isConnected = false;
		}

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
			// Receive a message on the created queue.
			if (xQueueReceive(subQueue, &(rxData), (TickType_t ) 1)) {
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
			} else {
				vTaskDelay(1);
			}
			updateGpio();
		} else {
			vTaskDelay(1);
		}

		checkButton();
		taskYIELD();
	}
	vTaskDelete(NULL);
}

void gpio_task_setup(void) {
	gpio_config_t config = {.pin_bit_mask = (
			CONTROL0_BIT|CONTROL1_BIT|CONTROL2_BIT|CONTROL3_BIT|LED_GPIO_BIT),
			.mode = GPIO_MODE_OUTPUT};
	gpio_config(&config);
	for (int i=0; i< (sizeof(gpioLut)/sizeof(gpioLut[0]));i++) {
		gpio_set_level(gpioLut[i], 1);
	}
	LED_OFF();

	// Create a queue capable of containing 10 messages.
	subQueue = xQueueCreate(10, sizeof(queueData_t));
	if (subQueue == 0) {
		// Queue was not created and must not be used.
		ESP_LOGI(TAG, "subqueue init failure");
	}

	xTaskCreate(&gpio_task, "gpio_task", 2048, NULL, 5, NULL);
}

void gpio_onConnect(void) {
	/* send status of all avail channels */
	queueData_t data = { 0, mStatus };
	for (int i = 0; i < (sizeof(gpioLut)/sizeof(gpioLut[0])); i++) {
		data.chan = i;
		sendStatus(&data);
	}
}

static void sendStatus(const queueData_t* pData) {

	cJSON *root = NULL;

	if (pData && pData->chan != -1) {

		root = cJSON_CreateObject();
		if (root == NULL) {
			goto end;
		}

		cJSON *channel = cJSON_AddObjectToObject(root, "channel");
		if (channel == NULL) {
			goto end;
		}

		if (cJSON_AddNumberToObject(channel, "num", pData->chan) == NULL) {
			goto end;
		}

		if (cJSON_AddNumberToObject(channel, "val",
				pData->mode == mOn ? 1 : 0) == NULL) {
			goto end;
		}

		char *string = cJSON_Print(root);
		if (string == NULL) {
			fprintf(stderr, "Failed to print channel.\n");
			goto end;
		}

		message_t message = {
				.pTopic = (char*) getPubMsg(),
				.topic_len = 0,
				.pData = string,
				.data_len = strlen(string)};

		if ( xQueueSendToBack(pubQueue, &message, 10) != pdPASS) {
			// Failed to post the message, even after 10 ticks.
			ESP_LOGI(TAG, "pubqueue post failure");
		}

		end: cJSON_Delete(root);
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
