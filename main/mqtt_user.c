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
#include "gpioTask.h"

static const char *TAG = "MQTTS";
static const char* chanNames[] = { "channel0", "channel1", "channel2",
		"channel3" };
static const int maxChanIndex = sizeof(chanNames) / sizeof(chanNames[0]);

char mqtt_sub_msg[64] = { 0 };
char mqtt_pub_msg[64] = { 0 };

esp_mqtt_client_handle_t client = NULL;

static void mqtt_message_handler(esp_mqtt_event_handle_t event);
static int handleControlMsg(esp_mqtt_event_handle_t event);
static void handleChannelControl(cJSON* chan);

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event) {
	client = event->client;
	int msg_id;
	// your_context_t *context = event->context;
	switch (event->event_id) {
	case MQTT_EVENT_CONNECTED:
		ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED")
		;
		/* send status of all avail channels */
		queueData_t data = { 0, mStatus };
		for (int i = 0; i < maxChanIndex; i++) {
			data.chan = i;
			if ( xQueueSend(subQueue, (void * ) &data,
					(TickType_t ) 10) != pdPASS) {
				// Failed to post the message, even after 10 ticks.
				ESP_LOGI(TAG, "subqueue post failure");
			}
		}
		msg_id = esp_mqtt_client_subscribe(client, mqtt_sub_msg, 1);
		ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id)
		;
		break;
	case MQTT_EVENT_DISCONNECTED:
		ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED")
		;
		break;

	case MQTT_EVENT_SUBSCRIBED:
		ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id)
		;
		break;
	case MQTT_EVENT_UNSUBSCRIBED:
		ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id)
		;
		break;
	case MQTT_EVENT_PUBLISHED:
		ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id)
		;
		break;
	case MQTT_EVENT_DATA:
		ESP_LOGI(TAG, "MQTT_EVENT_DATA")
		;
		mqtt_message_handler(event);
		break;
	case MQTT_EVENT_ERROR:
		ESP_LOGI(TAG, "MQTT_EVENT_ERROR")
		;
		break;
	}
	return ESP_OK;
}

static void mqtt_message_handler(esp_mqtt_event_handle_t event) {

//	size_t len = event->data_len < 50 ? event->data_len : 50;
//	ESP_LOGI(TAG, "Topic received!: (%d) %.*s (%d) %.*s", event->topic_len,
//			event->topic_len, event->topic, event->data_len, len, event->data);
	ESP_LOGI(TAG, "Topic received!: (%d) %.*s", event->topic_len,
			event->topic_len, event->topic);

	if (handleControlMsg(event)) {

	} else if (handleOtaMessage(event)) {

	}
}


static int handleControlMsg(esp_mqtt_event_handle_t event) {
	int ret = 0;
	if (event->topic_len > strlen(mqtt_sub_msg)) {
		const char* pTopic = &event->topic[strlen(mqtt_sub_msg) - 1];
		ESP_LOGI(TAG, "%.*s", event->topic_len - strlen(mqtt_sub_msg) + 1,
				pTopic);
		//check for control messages
		if (strcmp(pTopic, "control") == 0) {
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

		ESP_LOGI(TAG, "channel :%d found ->%d", chanNum, chanVal);
		gpio_task_mode_t func = mStatus;

		if (chanVal == 1) {
			func = mOn;
		} else if (chanVal == 0) {
			func = mOff;
		}

		queueData_t cdata = { chanNum, func };
		// available if necessary.
		if (xQueueSend(subQueue, (void * ) &cdata,
				(TickType_t ) 10) != pdPASS) {
			// Failed to post the message, even after 10 ticks.
			ESP_LOGI(TAG, "subqueue post failure");
		}
	}
}

void mqtt_task(void *pvParameters) {

	while (1) {
		// check if we have something to do
		queueData_t rxData = { 0 };

		if ((client != NULL)
				&& (xQueueReceive(pubQueue, &(rxData), (TickType_t ) 10))) {

			ESP_LOGI(TAG, "pubQueue work");

			int chan = rxData.chan;
			cJSON *root = NULL;

			if (chan != -1 && (chan < maxChanIndex)) {

				root = cJSON_CreateObject();
				if (root == NULL) {
					goto end;
				}

				cJSON *channel = cJSON_CreateObject();
				if (channel == NULL) {
					goto end;
				}
				cJSON_AddItemToObject(root, "channel", channel);

				cJSON *chanNum = cJSON_CreateNumber(chan);
				if (chanNum == NULL) {
					goto end;
				}
				cJSON_AddItemToObject(channel, "num", chanNum);

				cJSON *chanVal = cJSON_CreateNumber(rxData.mode == mOn ? 1 : 0);
				if (chanVal == NULL) {
					goto end;
				}
				cJSON_AddItemToObject(channel, "val", chanVal);

				char *string = cJSON_Print(root);
				if (string == NULL) {
					fprintf(stderr, "Failed to print channel.\n");
					goto end;
				}

				ESP_LOGI(TAG, "publish %.*s : %.*s", strlen(mqtt_pub_msg),
						mqtt_pub_msg, strlen(string), string);

				int msg_id = esp_mqtt_client_publish(client, mqtt_pub_msg,
						string, strlen(string) + 1, 1, 0);
				ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
				free(string);
			}
			end:
			cJSON_Delete(root);
		}
		vTaskDelay(10);
		taskYIELD();
	}
	vTaskDelete(NULL);
}

static void mqtt_app_start(void) {
	const esp_mqtt_client_config_t mqtt_cfg = { .uri = MQTT_SERVER,
			.event_handle = mqtt_event_handler,
			// .user_context = (void *)your_context
			.buffer_size = 1024
	};

	client = esp_mqtt_client_init(&mqtt_cfg);
	esp_mqtt_client_start(client);
}

void mqtt_user_init(void) {
	uint8_t mac[6] = { 0 };
	esp_efuse_mac_get_default(mac);
	snprintf(mqtt_sub_msg, sizeof(mqtt_sub_msg), MQTT_PUB_MESSAGE_FORMAT, mac[5], mac[4], "#");
	snprintf(mqtt_pub_msg, sizeof(mqtt_pub_msg), MQTT_PUB_MESSAGE_FORMAT, mac[5], mac[4], "state/");

	ESP_LOGI(TAG, "sub: %s, pub: %s", mqtt_sub_msg, mqtt_pub_msg);

	xTaskCreatePinnedToCore(&mqtt_task, "mqtt_task", 2 * 8192, NULL, 5, NULL, 0);
	xTaskCreatePinnedToCore(&mqtt_ota_task, "mqtt_ota_task", 2 * 8192, NULL, 20, NULL, 0);

	mqtt_app_start();
}

