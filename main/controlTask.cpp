/*
 * gpioTask.c
 *
 *  Created on: 06.05.2018
 *      Author: marco
 */

#include <time.h>
#include <assert.h>
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

#include "status.h"
#include "controlTask.h"
#include "config_user.h"
#include "config.h"

#define TAG "gpio_task"

#define LEDC_RESOLUTION        LEDC_TIMER_13_BIT
#define LEDC_MAX			   ((1<<LEDC_RESOLUTION)-1)
#define LED_C_OFF			   LEDC_MAX

#if CHANOUT_USE_PWM==y
#define LED_C_HALF			   0
#define LED_C_ON			   ((LEDC_MAX*CONFIG_CHANOUT_PWM_DUTY)/100)
#define LED_C_TIME			   (0.25)
#define LED_FREQ			   (CONFIG_CHANOUT_PWM_FREQ)
#else
#define LED_C_HALF			   0
#define LED_C_ON			   0
#define LED_C_TIME			   1
#define LED_FREQ			   50
#endif
/**/
QueueHandle_t subQueue;
/**/
messageHandler_t controlHandler = {//
		.topicName = "/control",//
		.onMessage = handleControlMsg,//
		"control event" };
/**/
static channelSet_t chanMode[NUM_CONTROL]; //FIXME chanConfig.count()
static bool isConnected = false;
static ledc_channel_config_t ledc_channel[NUM_CONTROL] = { //FIXME chanConfig.count()
		{
				.gpio_num = CONTROL0_PIN, //
				.speed_mode = LEDC_LOW_SPEED_MODE, //
				.channel = LEDC_CHANNEL_0, //
				.intr_type = LEDC_INTR_DISABLE,//
				.timer_sel = LEDC_TIMER_0 , //
				.duty = LED_C_OFF, //
				.hpoint = 0, //
		},
		{
				.gpio_num = CONTROL1_PIN, //
				.speed_mode = LEDC_LOW_SPEED_MODE, //
				.channel = LEDC_CHANNEL_1, //
				.intr_type = LEDC_INTR_DISABLE,//
				.timer_sel = LEDC_TIMER_0, //
				.duty = LED_C_OFF, //
				.hpoint = 0,		//
		},
		{
				.gpio_num = CONTROL2_PIN, //
				.speed_mode = LEDC_LOW_SPEED_MODE, //
				.channel = LEDC_CHANNEL_2, //
				.intr_type = LEDC_INTR_DISABLE,//
				.timer_sel = LEDC_TIMER_0, ///
				.duty = LED_C_OFF, //
				.hpoint = 0, //
		},
		{
				.gpio_num = CONTROL3_PIN, //
				.speed_mode = LEDC_LOW_SPEED_MODE, //
				.channel = LEDC_CHANNEL_3, //
				.intr_type = LEDC_INTR_DISABLE,//
				.timer_sel = LEDC_TIMER_0 ,
				.duty = LED_C_OFF, //
				.hpoint = 0, //
		}
}; //

/**/
static void updateStatus(void);
static void disableChan(uint32_t chan);
static void enableChan(uint32_t chan);
static void updateChannel(uint32_t chan);
static int checkButton();
/**/
int handleControlMsg(const char * topic, esp_mqtt_event_handle_t event) {
	int ret = 0;
	if (isTopic(event, topic)) {
		ESP_LOGI(TAG, "%.*s", event->topic_len, event->topic);

		cJSON *root = cJSON_Parse(event->data);
		if (root != NULL) {
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
 {
 "channel": { "channel1": { "val": 1 } }
 }
 */
void handleChannelControl(const cJSON* const chan) {
	for (uint32_t i = 0; i < chanConfig.count(); i++) {
		cJSON* pChanObj = cJSON_GetObjectItem(chan, chanConfig.getName(i));
		if (pChanObj != NULL) {

			int chanVal = -1;
			cJSON* pJsonChanVal = cJSON_GetObjectItem(pChanObj, "val");
			if (pJsonChanVal != NULL) {
				if (cJSON_IsString(pJsonChanVal)) {
					const char * pS = pJsonChanVal->valuestring;
					if (strncmp(pS, "ON", 2) == 0) {
						chanVal = 1;
					} else {
						chanVal = 0;
					}
				}
			}

			ESP_LOGD(TAG, "channel :%d found ->%s", i, chanVal == 1 ? "on" : "off");
			gpio_task_mode_t func = mStatus;

			if (chanVal == 1) {
				func = mOn;
			} else if (chanVal == 0) {
				func = mOff;
			} else {
				func = mStatus;
			}

			queueData_t cdata = { i, func };
			if (xQueueSend(subQueue, (void * ) &cdata, 10 / portTICK_PERIOD_MS) != pdPASS) {
				// Failed to post the message, even after 10 ticks.
				ESP_LOGW(TAG, "subqueue post failure");
			}
		}
	}
}

void gpio_task(void *pvParameters) {

	while (1) {

		if (!isConnected && (xEventGroupGetBits(main_event_group) & MQTT_CONNECTED_BIT)) {
			isConnected = true;
			updateStatus();
		}

		if (!(xEventGroupGetBits(main_event_group) & MQTT_CONNECTED_BIT)) {
			isConnected = false;
		}

		// timekeeping for auto off
		for (int i = 0; i < chanConfig.count(); i++) {
			if (chanMode[i].mode != pOFF) {
				time_t now;
				// test for duty cycle switch
				time(&now);
				if ((difftime(now, chanMode[i].time) >= LED_C_TIME) && (chanMode[i].mode != pON)) {
					chanMode[i].mode = pON;
					updateChannel(i);
				}
				// test for auto off
				time(&now);
				if (difftime(now, chanMode[i].time) > chanConfig.getTime(i)) {
					chanMode[i].mode = pOFF;
					disableChan(i);
				}
			}
		}

		if (subQueue != 0) {
			queueData_t rxData;
			// Receive a message on the created queue.
			if (xQueueReceive(subQueue, &(rxData), (TickType_t ) 1)) {
				ESP_LOGI(TAG, "received %08X, %d", rxData.chan, rxData.mode);

				switch (rxData.mode) {
				case mStatus:
					if (rxData.chan < chanConfig.count()) {
						if (chanMode[rxData.chan].mode != pOFF) {
							rxData.mode = mOn;
						} else {
							rxData.mode = mOff;
						}
					}
					updateStatus();
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

	ledc_timer_config_t ledc_timer = { //
			.speed_mode = LEDC_LOW_SPEED_MODE,           // timer mode
			.duty_resolution = LEDC_TIMER_13_BIT, // resolution of PWM duty
			.timer_num = LEDC_TIMER_0,            // timer index
			.freq_hz = LED_FREQ,                      // frequency of PWM signal
			.clk_cfg = LEDC_AUTO_CLK };
	// Set configuration of timer0 for high speed channels

	ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
	assert(NUM_CONTROL >= chanConfig.count());

	// Set LED Controller with previously prepared configuration
	for (int ch = 0; ch < chanConfig.count(); ch++) {
		ledc_channel_config(&ledc_channel[ch]);
		ledc_set_duty(ledc_channel[ch].speed_mode, ledc_channel[ch].channel, ledc_channel[ch].duty);
		ledc_update_duty(ledc_channel[ch].speed_mode, ledc_channel[ch].channel);
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

void addChannelStatus(cJSON *root) {

	if (root == NULL) {
		return;
	}

	cJSON *channel = cJSON_AddObjectToObject(root, "channel");
	if (channel == NULL) {
		return;
	}

	for (int i = 0; i < chanConfig.count(); i++) {
		cJSON *channelv = cJSON_AddObjectToObject(channel, chanConfig.getName(i));
		if (channelv == NULL) {
			return;
		}

		if (cJSON_AddStringToObject(channelv, "val", chanMode[i].mode != pOFF ? "ON" : "OFF") == NULL) {
			return;
		}
	}

	return;
}

static void updateStatus(void) {
	if (status_event_group != NULL) {
		xEventGroupSetBits(status_event_group, STATUS_EVENT_CONTROL);
	}
}

static void enableChan(uint32_t chan) {
	if (chan < chanConfig.count()) {
		// single channel mode, disable all other
		for (int i = 0; i < chanConfig.count(); i++) {
			if ((i != chan) && chanMode[i].mode != pOFF) {
				disableChan(i);
			}
		}
		// enable if not enabled
		if (chanMode[chan].mode == pOFF) {
			chanMode[chan].mode = pHALF;
			updateChannel(chan);
		}
		updateStatus();
		time(&chanMode[chan].time);
	}
}

static void disableChan(uint32_t chan) {
//	if ((chan < chanConfig.count()) && (chanMode[chan].mode != pOFF)) {
	if ((chan < chanConfig.count())) {
		chanMode[chan].mode = pOFF;
		updateChannel(chan);
		updateStatus();
		time(&chanMode[chan].time);
	}
}

static void updateChannel(uint32_t chan) {
	if ((chan < chanConfig.count())) {
		ESP_LOGI(TAG, "update channel %d -> %d", chan, chanMode[chan].mode);
		switch (chanMode[chan].mode) {
		case pON:
			ledc_set_duty(ledc_channel[chan].speed_mode, ledc_channel[chan].channel,
			LED_C_ON);
			break;
		case pHALF:
			ledc_set_duty(ledc_channel[chan].speed_mode, ledc_channel[chan].channel,
			LED_C_HALF);
			break;
		case pOFF:
			ledc_set_duty(ledc_channel[chan].speed_mode, ledc_channel[chan].channel,
			LED_C_OFF);
			break;
		}
		ledc_update_duty(ledc_channel[chan].speed_mode, ledc_channel[chan].channel);
	}
}

static int checkButton() {
	static int wps_button_count = 0;
	if ((gpio_get_level((gpio_num_t)WPS_BUTTON) == 0)) {
		wps_button_count++;
		if (wps_button_count > (WPS_LONG_MS / portTICK_PERIOD_MS)) {
			xEventGroupSetBits(main_event_group, WPS_LONG_BIT);
			wps_button_count = 0;
		}
	} else {
		if (wps_button_count > (WPS_SHORT_MS / portTICK_PERIOD_MS)) {
			xEventGroupSetBits(main_event_group, WPS_SHORT_BIT);
		}
		wps_button_count = 0;
	}
	return wps_button_count;
}

