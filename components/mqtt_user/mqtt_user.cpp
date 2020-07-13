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
#include "mqtt_user.h"
#include "mqtt_user_ota.h"

#include "esp_log.h"

static const char *TAG = "MQTTS";

extern EventGroupHandle_t main_event_group;

static void handleFirmwareMsg(cJSON* firmware);

namespace mqtt {
/*
FIXME int handleSysMessage(const char * topic, esp_mqtt_event_handle_t event);
FIXME int handleConfigMsg(const char * topic, esp_mqtt_event_handle_t event);

messageHandler_t mqttConfigHandler = { //
		.topicName = "/config", //
				.onMessage = handleConfigMsg, //
				"config event" };

messageHandler_t sysConfigHandler = { //
		.topicName = "/system", //
				.onMessage = handleSysMessage, //
				"sys event" };
*/
 esp_err_t MqttUserTask::mqtt_event_handler(esp_mqtt_event_handle_t event) {
	esp_mqtt_client_handle_t client = event->client;
	MqttUserTask *mqtt = reinterpret_cast<MqttUserTask*> (event->user_context);
	int msg_id;
	switch (event->event_id) {
	case MQTT_EVENT_CONNECTED:
		ESP_LOGD(TAG, "MQTT_EVENT_CONNECTED");
		xEventGroupSetBits(main_event_group, MQTT_CONNECTED_BIT);
		msg_id = esp_mqtt_client_subscribe(client, mqttConf.getSubMsg(), 1);
		ESP_LOGD(TAG, "sent subscribe successful, msg_id=%d", msg_id);
		mqtt->isMqttConnected = true;
		break;
	case MQTT_EVENT_DISCONNECTED:
		xEventGroupClearBits(main_event_group, MQTT_CONNECTED_BIT);
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
		mqtt->handler(event);
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

void MqttUserTask::handler(esp_mqtt_event_handle_t event) {

	if (event->data_len < 64) {
		ESP_LOGD(TAG, "Topic received!: (%d) %.*s (%d) %.*s", event->topic_len, event->topic_len, event->topic,
				event->data_len, event->data_len, event->data);
	} else {
		ESP_LOGD(TAG, "Topic received!: (%d) %.*s", event->topic_len, event->topic_len, event->topic);
	}

	for (int i = 0; i < (sizeof(messageHandle) / sizeof(messageHandle[0])); i++) {
		/*
		if ((messageHandle[i] != NULL) && (messageHandle[i]->onMessage != NULL)) {
			if (messageHandle[i]->onMessage(messageHandle[i]->topicName, event)) {
				ESP_LOGI(TAG, "%s handled", messageHandle[i]->handlerName);
				break;
			}
		}
		*/
		if ((messageHandle[i].handler != NULL) && isTopic(event, messageHandle[i].topicName)) {
			messageHandle[i].handler->parse(event->data);
		}
	}
}

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
static int md5StrToAr(char* pMD5, uint8_t* md5) {
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

/*
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
void MqttUserTask::task() {
	const TickType_t xTicksToWait = 10 / portTICK_PERIOD_MS;

	while (1) {

		message_t rxData;

		if ((client != NULL) && (m_pubQueue.pop((rxData), xTicksToWait))) {

			if (isMqttConnected) {
				ESP_LOGD(TAG, "publish %.*s : %.*s", strlen(rxData.pTopic), rxData.pTopic, strlen(rxData.pData),
					rxData.pData);

				int msg_id = esp_mqtt_client_publish(client, rxData.pTopic, rxData.pData, strlen(rxData.pData) + 1, 1, 0);

				ESP_LOGD(TAG, "sent publish successful, msg_id=%d", msg_id);
			}

			if (rxData.topic_len > 0) {
				free(rxData.pTopic);
			}
			if (rxData.data_len > 0) {
				free(rxData.pData);
			}
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
		ESP_LOGE(TAG,"starting client");
		esp_mqtt_client_start(client);
	} else {
		ESP_LOGE(TAG,"mqtt connect failed, client not initialized");
	}
}

void MqttUserTask::disconnect(void) {
	if (client != NULL && isMqttConnected)
		esp_mqtt_client_stop(client);
}

int MqttUserTask::addHandler(const char *topic, Config::ParseHandler *pHandle) {
	int ret = 0;
	for (int i = 0; i < (sizeof(messageHandle) / sizeof(messageHandle[0])); i++) {
		if (messageHandle[i].handler == NULL) {
			messageHandle[i] = {topic,pHandle};
			ret = 1;
			break;
		}
	}
	if (ret) {
		ESP_LOGD(TAG, "added mqtt handler %s", pHandle->name());
	}
	return ret;
}

bool isTopic(esp_mqtt_event_handle_t event, const char * pCommand) {
	const char* psTopic = event->topic_len >= strlen(mqttConf.getSubMsg()) ? &event->topic[strlen(mqttConf.getSubMsg()) - 2] : "";
	return strncmp(psTopic, pCommand, strlen(pCommand)) == 0;
}
}
