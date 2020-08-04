/*
 * mqtt_client.c
 *
 *  Created on: 28.06.2018
 *      Author: marco
 */

#include <string.h>
#include <stdlib.h>

#include "sdkconfig.h"

#include "TaskCPP.h"
#include "SemaphoreCPP.h"
#include "QueueCPP.h"
#include "TimerCPP.h"
#include "freertos/event_groups.h"

#include "cJSON.h"
#include "config.h"
#include "config_user.h"

#include "mqtt_client.h"
#include "messager.h"
#include "otaWorker.h"
#include "esp_log.h"
#include "mqttUserTask.h"
#include "MainClass.h"

static const char *TAG = "MQTTS";

namespace mqtt {

 esp_err_t MqttUserTask::mqtt_event_handler(esp_mqtt_event_handle_t event) {
	esp_mqtt_client_handle_t client = event->client;
	MqttUserTask *mqtt = reinterpret_cast<MqttUserTask*> (event->user_context);
	int msg_id;
	switch (event->event_id) {
	case MQTT_EVENT_CONNECTED:
		ESP_LOGD(TAG, "MQTT_EVENT_CONNECTED");
		xEventGroupSetBits(MainClass::instance()->eventGroup(), MQTT_CONNECTED_BIT);
		msg_id = esp_mqtt_client_subscribe(client, mqttConf.getSubMsg(), 1);
		ESP_LOGD(TAG, "sent subscribe successful, msg_id=%d", msg_id);
		mqtt->isMqttConnected = true;
		break;
	case MQTT_EVENT_DISCONNECTED:
		xEventGroupClearBits(MainClass::instance()->eventGroup(), MQTT_CONNECTED_BIT);
		ESP_LOGD(TAG, "MQTT_EVENT_DISCONNECTED");
		mqtt->isMqttConnected = false;
		mqtt->isMqttInit = false;
		break;
	case MQTT_EVENT_SUBSCRIBED:
		ESP_LOGD(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
		break;
	case MQTT_EVENT_UNSUBSCRIBED:
		ESP_LOGD(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
		break;
	case MQTT_EVENT_PUBLISHED:
		ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
		break;
	case MQTT_EVENT_DATA:
		ESP_LOGD(TAG, "MQTT_EVENT_DATA");
		MainClass::instance()->getMessager().handle(event);
		break;
	case MQTT_EVENT_ERROR:
		ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
		break;
	case MQTT_EVENT_BEFORE_CONNECT:
		ESP_LOGE(TAG, "MQTT_EVENT_BEFORE_CONNECT");
		break;
	case MQTT_EVENT_ANY:
		ESP_LOGE(TAG, "MQTT_EVENT_ANY");
	}
	return ESP_OK;
}

void MqttUserTask::send(MqttQueueType rx) {
	send(rx.get());
	//m_pubQueue.add((rx), std::chrono::duration_cast<portTick>(std::chrono::milliseconds(10)).count());
}

void MqttUserTask::send(mqttMessage *rxData) {
	if ((client != NULL) && isMqttConnected) {
		ESP_LOGD(TAG, "publish %.*s : %.*s", rxData->m_topic.length(), rxData->m_topic.c_str(), rxData->m_data.length(),
				rxData->m_data.c_str());
		int msg_id = esp_mqtt_client_publish(client, rxData->m_topic.c_str(), rxData->m_data.c_str(),
				rxData->m_data.length() + 1, 1, 0);
		ESP_LOGD(TAG, "sent publish successful, msg_id=%d", msg_id);
	}
}

void MqttUserTask::task() {
	const TickType_t xTicksToWait = std::chrono::duration_cast<portTick>(std::chrono::milliseconds(10)).count();

	while (1) {

		MqttQueueType rxData;

		if ( (m_pubQueue.pop(rxData, xTicksToWait))) {

			send(rxData.get());
		}

		vTaskDelay(10 / portTICK_PERIOD_MS);
		taskYIELD();
	}
	vTaskDelete(NULL);
}

void MqttUserTask::init(void) {

	esp_mqtt_client_config_t mqtt_cfg;
	memset(&mqtt_cfg,0,sizeof(mqtt_cfg));
	mqtt_cfg.uri = mqttConf.getMqttServer();
	mqtt_cfg.event_handle = mqtt_event_handler;
	mqtt_cfg.username = mqttConf.getMqttUser();
	mqtt_cfg.password = mqttConf.getMqttPass();
	mqtt_cfg.user_context = this;

	client = esp_mqtt_client_init(&mqtt_cfg);
}

void MqttUserTask::connect(void) {
	if (client != NULL) {
		ESP_LOGD(TAG,"starting client");
		esp_mqtt_client_start(client);
	} else {
		ESP_LOGE(TAG,"mqtt connect failed, client not initialized");
	}
}

void MqttUserTask::disconnect(void) {
	ESP_LOGE(TAG,"stopping client");

	if (client != NULL)
		esp_mqtt_client_stop(client);
}


}
