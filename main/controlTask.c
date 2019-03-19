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
#include "driver/ledc.h"

#include "controlTask.h"
#include "config_user.h"

#define TAG "gpio_task"

#define LEDC_RESOLUTION        LEDC_TIMER_13_BIT
#define LED_C_OFF			   ((1<<LEDC_RESOLUTION)-1)
#define LED_C_HALF			   0
#define LED_C_ON			   0
#define LED_C_TIME			   1000


messageHandler_t controlHandler = {.pUserctx = NULL, .onMessage = handleControlMsg};

//static uint32_t actChan = 0;
typedef enum {pOFF, pHALF, pON} channelMode_t;
static struct {
	channelMode_t mode;
	time_t time;
} chanMode[NUM_CONTROL] = {0};

static bool isConnected = false;

static void handleChannelControl(cJSON* chan);
static void sendStatus(const queueData_t* pData);
static void disableChan(uint32_t chan);
static void enableChan(uint32_t chan);
static void updateChannel(uint32_t chan);
static int32_t bitToIndex(uint32_t bit);

static ledc_channel_config_t ledc_channel[NUM_CONTROL] = {
    {
        .channel    = LEDC_CHANNEL_0,
        .duty       = LED_C_OFF,
        .gpio_num   = CONTROL0_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER_0
    },
    {
        .channel    = LEDC_CHANNEL_1,
        .duty       = LED_C_OFF,
        .gpio_num   = CONTROL1_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER_0
    },
    {
        .channel    = LEDC_CHANNEL_2,
        .duty       = LED_C_OFF,
        .gpio_num   = CONTROL2_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER_0
    },
    {
        .channel    = LEDC_CHANNEL_3,
        .duty       = LED_C_OFF,
        .gpio_num   = CONTROL3_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER_0
    },
};

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
		for (int i = 0; i < NUM_CONTROL; i++) {
			if (chanMode[i].mode != pOFF) {
				time_t now;
				// test for duty cycle switch
				time(&now);
				if ((difftime(now, chanMode[i].time) > LED_C_TIME) && (chanMode[i].mode != pON)) {
					chanMode[i].mode = pON;
					updateChannel(i);
				}
				// test for auto off
				time(&now);
				if (difftime(now, chanMode[i].time) > AUTO_OFF_SEC) {
					chanMode[i].mode = pOFF;
					disableChan(i);
				}
			}
		}

		if (subQueue != 0) {
			queueData_t rxData = { 0 };
			// Receive a message on the created queue.
			if (xQueueReceive(subQueue, &(rxData), (TickType_t ) 1)) {
				ESP_LOGI(TAG, "received %08X, %d", rxData.chan, rxData.mode);

				switch (rxData.mode) {
				case mStatus:
					if (rxData.chan < NUM_CONTROL) {
						if (chanMode[rxData.chan].mode != pOFF) {
							rxData.mode = mOn;
						} else {
							rxData.mode = mOff;
						}
					}
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

		checkButton();
		vTaskDelay(10 / portTICK_PERIOD_MS);
	}
	vTaskDelete(NULL);
}

void gpio_task_setup(void) {

	ledc_timer_config_t ledc_timer = {
	        .duty_resolution = LEDC_TIMER_13_BIT, // resolution of PWM duty
	        .freq_hz = 50,                      // frequency of PWM signal
	        .speed_mode = LEDC_LOW_SPEED_MODE,           // timer mode
	        .timer_num = LEDC_TIMER_0            // timer index
	    };
	    // Set configuration of timer0 for high speed channels

	ledc_timer_config(&ledc_timer);


    // Set LED Controller with previously prepared configuration
    for (int ch = 0; ch < NUM_CONTROL; ch++) {
        ledc_channel_config(&ledc_channel[ch]);
        ledc_set_duty(
        		ledc_channel[ch].speed_mode,
        		ledc_channel[ch].channel,
				ledc_channel[ch].duty);
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
	for (int i = 0; i < NUM_CONTROL; i++) {
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
	if (chan < NUM_CONTROL) {
		// single channel mode, disable all other
		for (int i=0;i<NUM_CONTROL;i++) {
			if ((i != chan) && chanMode[i].mode != pOFF) {
				disableChan(i);
			}
		}
		// enable if not enabled
		if (chanMode[chan].mode == pOFF) {
			chanMode[chan].mode = pHALF;
			updateChannel(chan);
			queueData_t data = { bitToIndex(chan), mOn };
			sendStatus(&data);
		}
		time(&chanMode[chan].time);
	}
}

static void disableChan(uint32_t chan) {
	if ((chan < NUM_CONTROL) && (chanMode[chan].mode != pOFF)) {
		queueData_t data = {bitToIndex(chan), mOff};
		sendStatus(&data);
		chanMode[chan].mode = pOFF;
		updateChannel(chan);
		time(&chanMode[chan].time);
	}
}

static void updateChannel(uint32_t chan) {
	switch(chanMode[chan].mode){
	case pON:
	        ledc_set_duty(
	        		ledc_channel[chan].speed_mode,
	        		ledc_channel[chan].channel,
					LED_C_ON);
	        break;
	case pHALF:
	        ledc_set_duty(
	        		ledc_channel[chan].speed_mode,
	        		ledc_channel[chan].channel,
					LED_C_HALF);
	        break;
	case pOFF:
	        ledc_set_duty(
	        		ledc_channel[chan].speed_mode,
	        		ledc_channel[chan].channel,
					LED_C_OFF);
	        break;
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
