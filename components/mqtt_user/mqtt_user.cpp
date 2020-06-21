/*
 * mqtt_client.c
 *
 *  Created on: 28.06.2018
 *      Author: marco
 */

#include <string.h>
#include <stdlib.h>

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "cJSON.h"

#include "mqtt_client.h"
#include "mqtt_user.h"
#include "mqtt_user_ota.h"
#include "config.h"
#include "config_user.h"

static const char *TAG = "MQTTS";
static esp_mqtt_client_handle_t client = NULL;
static messageHandler_t* messageHandle[8] = { 0 };
static bool isMqttConnected = false;
static bool isMqttInit = false;

QueueHandle_t pubQueue, otaQueue;

static void mqtt_message_handler(esp_mqtt_event_handle_t event);
static void handleFirmwareMsg(cJSON* firmware);

int handleSysMessage(const char * topic, esp_mqtt_event_handle_t event);
int handleConfigMsg(const char * topic, esp_mqtt_event_handle_t event);

messageHandler_t mqttConfigHandler = { //
		.topicName = "/config", //
				.onMessage = handleConfigMsg, //
				"config event" };

messageHandler_t sysConfigHandler = { //
		.topicName = "/system", //
				.onMessage = handleSysMessage, //
				"sys event" };

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event) {
	client = event->client;
	int msg_id;
	switch (event->event_id) {
	case MQTT_EVENT_CONNECTED:
		ESP_LOGD(TAG, "MQTT_EVENT_CONNECTED");
		xEventGroupSetBits(main_event_group, MQTT_CONNECTED_BIT);
		msg_id = esp_mqtt_client_subscribe(client, mqtt.getSubMsg(), 1);
		ESP_LOGD(TAG, "sent subscribe successful, msg_id=%d", msg_id);
		isMqttConnected = true;
		break;
	case MQTT_EVENT_DISCONNECTED:
		xEventGroupClearBits(main_event_group, MQTT_CONNECTED_BIT);
		ESP_LOGD(TAG, "MQTT_EVENT_DISCONNECTED");
		isMqttConnected = false;
		isMqttInit = false;
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
		mqtt_message_handler(event);
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

static void mqtt_message_handler(esp_mqtt_event_handle_t event) {

	if (event->data_len < 64) {
		ESP_LOGD(TAG, "Topic received!: (%d) %.*s (%d) %.*s", event->topic_len, event->topic_len, event->topic,
				event->data_len, event->data_len, event->data);
	} else {
		ESP_LOGD(TAG, "Topic received!: (%d) %.*s", event->topic_len, event->topic_len, event->topic);
	}

	for (int i = 0; i < (sizeof(messageHandle) / sizeof(messageHandle[0])); i++) {
		if ((messageHandle[i] != NULL) && (messageHandle[i]->onMessage != NULL)) {
			if (messageHandle[i]->onMessage(messageHandle[i]->topicName, event)) {
				ESP_LOGI(TAG, "%s handled", messageHandle[i]->handlerName);
				break;
			}
		}
	}
}

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

int handleConfigMsg(const char * topic, esp_mqtt_event_handle_t event) {
	int ret = 0;
	if (isTopic(event, topic)) {
		ESP_LOGI(TAG, "%.*s", event->topic_len, event->topic);
		//FIXME updateConfig(event->data);
		ret = 1;
	}
	return ret;
}

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

void mqtt_task(void *pvParameters) {
	const TickType_t xTicksToWait = 10 / portTICK_PERIOD_MS;

	while (1) {

		message_t rxData;

		if ((client != NULL) && (xQueueReceive(pubQueue, &(rxData), xTicksToWait))) {

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

void mqtt_user_init(void) {

	pubQueue = xQueueCreate(10, sizeof(message_t));
	if (pubQueue == 0) {
		// Queue was not created and must not be used.
		ESP_LOGI(TAG, "pubqueue init failure");
	}

	otaQueue = xQueueCreate(2, sizeof(md5_update_t));
	if (otaQueue == 0) {
		// Queue was not created and must not be used.
		ESP_LOGI(TAG, "otaQueue init failure");
	}

	esp_mqtt_client_config_t mqtt_cfg;
	memset(&mqtt_cfg,0,sizeof(mqtt_cfg));
	mqtt_cfg.uri = mqtt.getMqttServer();
	mqtt_cfg.event_handle = mqtt_event_handler;
	mqtt_cfg.username = mqtt.getMqttUser();
	mqtt_cfg.password = mqtt.getMqttPass();

	client = esp_mqtt_client_init(&mqtt_cfg);

	xTaskCreatePinnedToCore(&mqtt_task, "mqtt_task", 2 * 8192, NULL, 5, NULL, 0);
	xTaskCreatePinnedToCore(&mqtt_ota_task, "mqtt_ota_task", 2 * 8192, NULL, 5, NULL, 0);

	mqtt_user_addHandler(&sysConfigHandler);
	mqtt_user_addHandler(&mqttConfigHandler);
	mqtt_user_addHandler(&mqttOtaHandler);
}

void mqtt_connect(void) {
	if (client != NULL)
		esp_mqtt_client_start(client);
}

void mqtt_disconnect(void) {
	if (client != NULL && isMqttConnected)
		esp_mqtt_client_stop(client);
}

int mqtt_user_addHandler(messageHandler_t *pHandle) {
	int ret = 0;
	for (int i = 0; i < (sizeof(messageHandle) / sizeof(messageHandle[0])); i++) {
		if (messageHandle[i] == NULL) {
			messageHandle[i] = pHandle;
			ret = 1;
			break;
		}
	}
	return ret;
}

bool isTopic(esp_mqtt_event_handle_t event, const char * pCommand) {
	const char* psTopic = event->topic_len >= strlen(mqtt.getSubMsg()) ? &event->topic[strlen(mqtt.getSubMsg()) - 2] : "";
	return strncmp(psTopic, pCommand, strlen(pCommand)) == 0;
}
