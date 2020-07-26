/*
 * ChannelAdapter.cpp
 *
 *  Created on: 23.07.2020
 *      Author: marco
 */

#include <stdio.h>
#include <time.h>
#include "esp_log.h"

#include "sdkconfig.h"
#include "config.h"
#include "config_user.h"
#include "channelAdapter.h"

#include "MainClass.h"

#define TAG "channelAdapter"

bool isTopic(esp_mqtt_event_handle_t event, const char * pCommand) {
	const char* psTopic = event->topic;//event->topic_len >= strlen(mqttConf.getSubMsg()) ? &event->topic[strlen(mqttConf.getSubMsg()) - 2] : "";
	ESP_LOGD(TAG,"isTopic: %.*s , %.*s", strlen(pCommand), psTopic, strlen(pCommand), pCommand);
	return strncmp(psTopic, pCommand, strlen(pCommand)) == 0;
}

int MqttChannelAdapter::onMessage(esp_mqtt_event_handle_t event) {
	if (event->data_len < 64) {
		ESP_LOGD(TAG, "Topic received!: (%d) %.*s (%d) %.*s", event->topic_len, event->topic_len, event->topic,
				event->data_len, event->data_len, event->data);
	} else {
		ESP_LOGD(TAG, "Topic received!: (%d) %.*s", event->topic_len, event->topic_len, event->topic);
	}
	//check message and set channel or notify
	return 0;
}

void MqttChannelAdapter::onNotify() {
}

int MqttJsonChannelAdapter::onMessage(esp_mqtt_event_handle_t event) {
	int ret = 0;
	if (isTopic(event, m_topic.c_str()))
	{
		ESP_LOGI(channel()->name(), "%.*s", event->topic_len, event->topic);

		cJSON *root = cJSON_Parse(event->data);
		if (root != NULL) {
			cJSON *chan = cJSON_GetObjectItem(root, "channel");
			if (chan != NULL) {
				cJSON *pChanObj = cJSON_GetObjectItem(chan, channel()->name());
				if (pChanObj != NULL) {
					int chanTime = -1;
					int status = -1;
					cJSON *pJsonChanVal = cJSON_GetObjectItem(pChanObj, "val");
					if (cJSON_IsString(pJsonChanVal)) {
						const char *pS = pJsonChanVal->valuestring;
						if (strncmp(pS, "ON", 2) == 0) {
							status = 1;
						} else {
							status = 0;
						}
					}

					cJSON *pJsonTime = cJSON_GetObjectItem(pChanObj, "time");
					if (cJSON_IsNumber(pJsonTime)) {
						chanTime = pJsonTime->valuedouble;
					}

					ESP_LOGD(TAG, "channel :%s found ->%s, time : %d", channel()->name(), status ==1 ? "on" : "off", chanTime);

					if(status == -1) {
						channel()->notify();
					} else {
						channel()->set(status, chanTime);
					}
					ret = 1;
				}

			}
			cJSON_Delete(root);
		}
	}
	return ret;
}

void MqttJsonChannelAdapter::onNotify() {
	cJSON *root = cJSON_CreateObject();
	//add time and date
	//TODO
	if (root != NULL) {
		cJSON *pChan = cJSON_AddObjectToObject(root, "channel");
		if (pChan != NULL) {
			cJSON *pChanv = cJSON_AddObjectToObject(pChan, channel()->name());
			if (pChanv != NULL) {
				cJSON_AddStringToObject(pChanv, "val", channel()->get() ? "ON" : "OFF");
			}
		}
		cJSON_Delete(root);
	}
	//forward to messager
	//MainClass::instance()->mqtt().send();
}
