/*
 * gpioTask.c
 *
 *  Created on: 06.05.2018
 *      Author: marco
 */

#include <time.h>
#include <assert.h>
#include "TaskCPP.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "sdkconfig.h"
#include "esp_log.h"
#include "cJSON.h"

#include "config_user.h"
#include "config.h"
#include "mqtt_config.h"
#include "mqtt_client.h"
#include "mqtt_user.h"

#include "status.h"

#include "controlTask.h"

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
extern GpioTask channel;

int handleControlMsg(const char *topic, esp_mqtt_event_handle_t event) {
	return (channel.handleControlMsg(topic, event));
}

/*
FIXME messageHandler_t controlHandler = { //
		.topicName = "/control", .onMessage = handleControlMsg, "control event" };
*/

/**/
GpioTask::channelSet_t GpioTask::chanMode[NUM_CONTROL];
/**/
ledc_channel_config_t GpioTask::ledc_channel[NUM_CONTROL] = { { //FIXME chanConfig.count()
		CONTROL0_PIN, //
				LEDC_LOW_SPEED_MODE, //
				LEDC_CHANNEL_0, LEDC_INTR_DISABLE, //
				LEDC_TIMER_0, //
				LED_C_OFF, //
				0, //
		}, { //
				CONTROL1_PIN, //
				LEDC_LOW_SPEED_MODE, //
				LEDC_CHANNEL_1, //
				LEDC_INTR_DISABLE, //
				LEDC_TIMER_0, //
				LED_C_OFF, //
				0,		//
		}, { //
				CONTROL2_PIN, //
				LEDC_LOW_SPEED_MODE, //
				LEDC_CHANNEL_2, //
				LEDC_INTR_DISABLE, //
				LEDC_TIMER_0, ///
				LED_C_OFF, //
				0, //
		}, { //
				CONTROL3_PIN, //
				LEDC_LOW_SPEED_MODE, //
				LEDC_CHANNEL_3, //
				LEDC_INTR_DISABLE, //
				LEDC_TIMER_0, LED_C_OFF, //
				0, //
		} }; //

GpioTask::GpioTask(EventGroupHandle_t &_main) :
		TaskClass("channel", TaskPrio_HMI, 2048), m_isConnected(false),
		m_update(false), m_sem("gpio"), m_subQueue(8), m_pMain(&_main) {
}

void GpioTask::task() {
	while (1) {
		if (*m_pMain != NULL) {
			if (!m_isConnected && (xEventGroupGetBits(*m_pMain) & MQTT_CONNECTED_BIT)) {
				m_isConnected = true;
				setUpdate(true);
			}

			if (!(xEventGroupGetBits(*m_pMain) & MQTT_CONNECTED_BIT)) {
				m_isConnected = false;
			}

			// timekeeping for auto off
			checkTimeout();

			queueData rxData;
			// Receive a message on the created queue.
			if (m_subQueue.pop(rxData, (TickType_t) 1)) {
				ESP_LOGI(TAG, "received %08X, %d", rxData.chan, rxData.mode);
				checkMessage(rxData);
			}

			checkWPSButton();

		}
		vTaskDelay(10 / portTICK_PERIOD_MS);
	}
}

void GpioTask::enableChan(uint32_t chan, time_t _runTime) {
	if (chan < chanConf.count()) {
		// single channel mode, disable all other
		for (int i = 0; i < chanConf.count(); i++) {
			if ((i != chan) && chanMode[i].mode != pOFF) {
				disableChan(i);
			}
		}
		// enable if not enabled
		if (chanMode[chan].mode == pOFF) {
			chanMode[chan].mode = pHALF;
			updateChannel(chan);
		}
		setUpdate(true);
		time(&chanMode[chan].startTime);
		chanMode[chan].runTime = _runTime;
	}
}

void GpioTask::disableChan(uint32_t chan) {
	if ((chan < chanConf.count())) {
		chanMode[chan].mode = pOFF;
		updateChannel(chan);
		setUpdate(true);
		time(&chanMode[chan].startTime);
		chanMode[chan].runTime = 0;
	}
}

void GpioTask::updateChannel(uint32_t chan) {
	if ((chan < chanConf.count())) {
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

GpioTask::~GpioTask() {
}

bool GpioTask::hasUpdate() {
	return false;
}

void GpioTask::addStatus(cJSON *root) {

	if (root == NULL) {
		return;
	}

	cJSON *channel = cJSON_AddObjectToObject(root, "channel");
	if (channel == NULL) {
		return;
	}

	for (int i = 0; i < chanConf.count(); i++) {
		cJSON *channelv = cJSON_AddObjectToObject(channel, chanConf.getName(i));
		if (channelv == NULL) {
			return;
		}

		if (cJSON_AddStringToObject(channelv, "val", chanMode[i].mode != pOFF ? "ON" : "OFF") == NULL) {
			return;
		}
	}

	return;
}
/**/
void GpioTask::checkMessage(queueData &rxData) {
	switch (rxData.mode) {
	case mStatus:
		if (rxData.chan < chanConf.count()) {
			if (chanMode[rxData.chan].mode != pOFF) {
				rxData.mode = mOn;
			} else {
				rxData.mode = mOff;
			}
		}
		setUpdate(true);
		break;
	case mOn:
		enableChan(rxData.chan, rxData.time);
		break;
	case mOff:
		disableChan(rxData.chan);
		break;
	}
}
/**/
void GpioTask::checkTimeout() {
	for (int i = 0; i < chanConf.count(); i++) {
		if (chanMode[i].mode != pOFF) {
			time_t now;
			// test for duty cycle switch
			time(&now);
			if ((difftime(now, chanMode[i].startTime) >= LED_C_TIME) && (chanMode[i].mode != pON)) {
				chanMode[i].mode = pON;
				updateChannel(i);
			}
			// test for auto off
			time(&now);
			if (difftime(now, chanMode[i].startTime) > chanMode[i].runTime) {
				chanMode[i].mode = pOFF;
				disableChan(i);
			}
		}
	}
}
/*
 * TODO: move function outside off GpioTask
 * */
int GpioTask::handleControlMsg(const char *topic, esp_mqtt_event_handle_t event) {
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
/*
 * TODO: move function outside off GpioTask
 * */
void GpioTask::handleChannelControl(const cJSON *const chan) {
	for (uint32_t i = 0; i < chanConf.count(); i++) {
		cJSON *pChanObj = cJSON_GetObjectItem(chan, chanConf.getName(i));
		if (pChanObj != NULL) {

			gpio_task_mode_t func = mStatus;
			uint32_t chanTime = chanConf.getTime(i);
			cJSON *pJsonChanVal = cJSON_GetObjectItem(pChanObj, "val");
			if (cJSON_IsString(pJsonChanVal)) {
				const char *pS = pJsonChanVal->valuestring;
				if (strncmp(pS, "ON", 2) == 0) {
					func = mOn;
				} else {
					func = mOff;
				}
			}

			cJSON *pJsonTime = cJSON_GetObjectItem(pChanObj, "time");
			if (cJSON_IsNumber(pJsonTime)) {
				chanTime = pJsonTime->valuedouble;
			}

			ESP_LOGD(TAG, "channel :%d found ->%s, time : %d", i, func == mOn ? "on" : "off", chanTime);

			queueData cdata = { i, func, chanTime };
			if (!m_subQueue.add(cdata, 10 / portTICK_PERIOD_MS)) {
				// Failed to post the message, even after 10 ticks.
				ESP_LOGW(TAG, "subqueue post failure");
			}
		}
	}
}

int GpioTask::checkWPSButton() {
	static int wps_button_count = 0;
	if ((gpio_get_level((gpio_num_t) WPS_BUTTON) == 0)) {
		wps_button_count++;
		if (wps_button_count > (WPS_LONG_MS / portTICK_PERIOD_MS)) {
			xEventGroupSetBits(*m_pMain, WPS_LONG_BIT);
			wps_button_count = 0;
		}
	} else {
		if (wps_button_count > (WPS_SHORT_MS / portTICK_PERIOD_MS)) {
			xEventGroupSetBits(*m_pMain, WPS_SHORT_BIT);
		}
		wps_button_count = 0;
	}
	return wps_button_count;
}

void GpioTask::setup(void) {

	ledc_timer_config_t ledc_timer = { //
			.speed_mode = LEDC_LOW_SPEED_MODE,           // timer mode
					.duty_resolution = LEDC_TIMER_13_BIT, // resolution of PWM duty
					.timer_num = LEDC_TIMER_0,            // timer index
					.freq_hz = LED_FREQ,                      // frequency of PWM signal
					.clk_cfg = LEDC_AUTO_CLK };
	// Set configuration of timer0 for high speed channels

	ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
	assert(NUM_CONTROL >= chanConf.count());

	// Set LED Controller with previously prepared configuration
	for (int ch = 0; ch < chanConf.count(); ch++) {
		ledc_channel_config(&ledc_channel[ch]);
		ledc_set_duty(ledc_channel[ch].speed_mode, ledc_channel[ch].channel, ledc_channel[ch].duty);
		ledc_update_duty(ledc_channel[ch].speed_mode, ledc_channel[ch].channel);
	}

	LED_OFF();
}

