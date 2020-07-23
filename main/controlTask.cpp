/*
 * gpioTask.c
 *
 *  Created on: 06.05.2018
 *      Author: marco
 */

#include <time.h>
#include <assert.h>
#include "TaskCPP.h"
#include "QueueCPP.h"
#include "SemaphoreCPP.h"
#include "TimerCPP.h"
#include "freertos/event_groups.h"

#include "sdkconfig.h"
#include "esp_log.h"
#include "cJSON.h"

#include "config_user.h"
#include "config.h"
#include "mqtt_config.h"
#include "mqtt_client.h"
#include "mqttUserTask.h"
#include "controlTask.h"

#include "channelFactory.h"
#include "statusTask.h"

#define TAG "gpio_task"
/**/

ControlTask::ControlTask(EventGroupHandle_t &_main) :
		TaskClass("channel", TaskPrio_HMI, 2048), m_isConnected(false), m_update(false), m_subQueue(), m_pMain(
				&_main), m_status(this) {
}

void ControlTask::task() {
	while (1) {
		if (*m_pMain != NULL) {
			if (!m_isConnected && (xEventGroupGetBits(*m_pMain) & MQTT_CONNECTED_BIT)) {
				m_isConnected = true;
				setUpdate(true);
			}

			if (!(xEventGroupGetBits(*m_pMain) & MQTT_CONNECTED_BIT)) {
				m_isConnected = false;
			}


			AbstractChannel *rxData;
			// Receive a message on the created queue.
			if (m_subQueue.pop(rxData, (TickType_t) 1)) {
//				ESP_LOGI(TAG, "received %08X, %d", rxData.chan, rxData.mode);
//				checkMessage(rxData);
			}

		}
		vTaskDelay(10 / portTICK_PERIOD_MS);
	}
}


ControlTask::~ControlTask() {
}

bool ControlTask::hasUpdate() {
	return false;
}

void ControlTask::addStatus(cJSON *root) {

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
/* TODO
		if (cJSON_AddStringToObject(channelv, "val", chanMode[i].mode != pOFF ? "ON" : "OFF") == NULL) {
			return;
		}
*/
	}

	return;
}
/**/

/**/


void ControlTask::setup(void) {

	LED_OFF();
}

