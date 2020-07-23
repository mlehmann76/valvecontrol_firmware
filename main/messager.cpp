/*
 * messageHandler.cpp
 *
 *  Created on: 20.07.2020
 *      Author: marco
 */

#include "messager.h"

#include <string.h>
#include <stdlib.h>

#include "sdkconfig.h"
#include "esp_log.h"
#include "channelAdapter.h"

static const char *TAG = "MESSAGER";



Messager::Messager() {
	// TODO Auto-generated constructor stub

}

Messager::~Messager() {
	// TODO Auto-generated destructor stub
}

void Messager::handle(esp_mqtt_event_handle_t event) {
	if (event->data_len < 64) {
		ESP_LOGD(TAG, "Topic received!: (%d) %.*s (%d) %.*s", event->topic_len, event->topic_len, event->topic,
				event->data_len, event->data_len, event->data);
	} else {
		ESP_LOGD(TAG, "Topic received!: (%d) %.*s", event->topic_len, event->topic_len, event->topic);
	}

	for (auto m : m_mqttAdapter) {
		if (m->onMessage(event)) {
			break;
		}
	}
}

void Messager::addHandle(MqttChannelAdapter* _a) {
	m_mqttAdapter.push_back(_a);
}
/*
 * static int md5StrToAr(char* pMD5, uint8_t* md5) {
	int error = 0;
	for (int i = 0; i < 32; i++) {
		int temp = 0;
		if (pMD5[i] <= '9' && pMD5[i] >= '0') {
			temp = pMD5[i] - '0';
		} else if (pMD5[i] <= 'f' && pMD5[i] >= 'a') {
			temp = pMD5[i] - 'a' + 10;
		} else if (pMD5[i] <= 'F' && pMD5[i] >= 'A') {
			temp = pMD5[i] - 'A' + 10;
		} else {
			error = 1;
			break;
		}

		if ((i & 1) == 0) {
			temp *= 16;
			md5[i / 2] = temp;
		} else {
			md5[i / 2] += temp;
		}
	}
	return error;
}
 *
static void handleFirmwareMsg(cJSON* firmware) {
	int error = 0;
	md5_update_t md5_update;
	char* pMD5 = NULL;
	const TickType_t xTicksToWait = 10 / portTICK_PERIOD_MS;
	cJSON* pfirmware = cJSON_GetObjectItem(firmware, "update");

	if (pfirmware != NULL) {
		char* pUpdate = cJSON_GetStringValue(cJSON_GetObjectItem(firmware, "update"));
		if (strcmp(pUpdate, "ota") != 0) {
			error = 1;
		}

		if (!error && cJSON_GetObjectItem(firmware, "md5") == NULL) {
			error = 2;
		} else {
			pMD5 = cJSON_GetStringValue(cJSON_GetObjectItem(firmware, "md5"));
		}

		if (!error && (strlen(pMD5) != 32)) {
			error = 3;
		}

		if (!error && md5StrToAr(pMD5, md5_update.md5) != 0) {
			error = 4;
		}

		if (!error && cJSON_GetObjectItem(firmware, "len") == NULL) {
			error = 5;
		} else {
			md5_update.len = cJSON_GetObjectItem(firmware, "len")->valueint;
		}

		if (error) {
			ESP_LOGE(TAG, "handleFirmwareMsg error %d", error);
		} else {
			ESP_LOGV(TAG, "fileSize :%d ->md5: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
					md5_update.len, md5_update.md5[0], md5_update.md5[1], md5_update.md5[2], md5_update.md5[3],
					md5_update.md5[4], md5_update.md5[5], md5_update.md5[6], md5_update.md5[7], md5_update.md5[8],
					md5_update.md5[9], md5_update.md5[10], md5_update.md5[11], md5_update.md5[12], md5_update.md5[13],
					md5_update.md5[14], md5_update.md5[15]);

			if (xQueueSend(otaQueue, (void * ) &md5_update,
					xTicksToWait) != pdPASS) {
				ESP_LOGW(TAG, "otaqueue post failure");
			}
		}
	}
}
*/
/*
int handleSysMessage(const char * topic, esp_mqtt_event_handle_t event) {
	int ret = 0;
	if (isTopic(event, topic)) {
		ESP_LOGI(TAG, "%.*s", event->topic_len, event->topic);
		cJSON *root = cJSON_Parse(event->data);
		cJSON *firmware = cJSON_GetObjectItem(root, "firmware");
		if (firmware != NULL) {
			handleFirmwareMsg(firmware);
		}
		cJSON_Delete(root);
		ret = 1;
	}
	return ret;
}
*/
/*
int handleConfigMsg(const char * topic, esp_mqtt_event_handle_t event) {
	int ret = 0;
	if (isTopic(event, topic)) {
		ESP_LOGI(TAG, "%.*s", event->topic_len, event->topic);
		//FIXME updateConfig(event->data);
		ret = 1;
	}
	return ret;
}
*/
/* */
/**
 * control format for channel control
 {
 "channel": { "channel1": { "val": 1 } }
 }
 */
/*
 * TODO: move function outside off GpioTask
 *
int Messager::handleControlMsg(const char *topic, esp_mqtt_event_handle_t event) {
	int ret = 0;
	//FIXME if (isTopic(event, topic))
	{
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
*/


/*
 * TODO: move function outside off GpioTask
 */
/*
void Messager::handleChannelControl(const cJSON *const chan) {
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
*/

