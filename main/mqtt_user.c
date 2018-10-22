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
static const int maxChanIndex = 4; //TODO

char mqtt_sub_msg[64] = { 0 }; //TODO
char mqtt_pub_msg[64] = { 0 }; //TODO

esp_mqtt_client_handle_t client = NULL;

static void mqtt_message_handler(esp_mqtt_event_handle_t event);
static int handleSysMessage(esp_mqtt_event_handle_t event);
static int handleControlMsg(esp_mqtt_event_handle_t event);
static void handleChannelControl(cJSON* chan);
static void handleFirmwareMsg(cJSON* firmware);

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event) {
	client = event->client;
	int msg_id;
	// your_context_t *context = event->context;
	switch (event->event_id) {
	case MQTT_EVENT_CONNECTED:
		ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
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

	ESP_LOGI(TAG, "Topic received!: (%d) %.*s", event->topic_len,
			event->topic_len, event->topic);

	if (handleSysMessage(event)) {

	} else if (handleOtaMessage(event)) {

	} else if (handleControlMsg(event)) {

	}
}
static int handleSysMessage(esp_mqtt_event_handle_t event) {
	int ret = 0;
	if (event->topic_len > strlen(mqtt_sub_msg)) {
		const char* pTopic = &event->topic[strlen(mqtt_sub_msg) - 1];
		//check for control messages
		if (strcmp(pTopic, "system") == 0) {
			ESP_LOGI(TAG, "%.*s", event->topic_len - strlen(mqtt_sub_msg) + 1,
					pTopic);
			cJSON *root = cJSON_Parse(event->data);
			cJSON *firmware = cJSON_GetObjectItem(root, "firmware");
			if (firmware != NULL) {
				handleFirmwareMsg(firmware);
			}
			cJSON_Delete(root);
			ret = 1;
		}
	}
	return ret;
}

/* */
int md5StrToAr(char* pMD5, uint8_t* md5) {
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
	char* pMD5;

	if (cJSON_GetObjectItem(firmware, "update") != NULL) {
		char* pUpdate = cJSON_GetStringValue(
				cJSON_GetObjectItem(firmware, "update"));
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

		if(!error && md5StrToAr(pMD5, md5_update.md5) !=0) {
			error = 4;
		}

		if (!error && cJSON_GetObjectItem(firmware, "len") == NULL) {
			error = 5;
		} else {
			md5_update.len = cJSON_GetObjectItem(firmware, "len")->valueint;
		}

		if (error) {
			ESP_LOGI(TAG, "handleFirmwareMsg error %d", error);
		} else {
			ESP_LOGI(TAG,
					"fileSize :%d ->md5: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
					md5_update.len, md5_update.md5[0], md5_update.md5[1],
					md5_update.md5[2], md5_update.md5[3], md5_update.md5[4],
					md5_update.md5[5], md5_update.md5[6], md5_update.md5[7],
					md5_update.md5[8], md5_update.md5[9], md5_update.md5[10],
					md5_update.md5[11], md5_update.md5[12], md5_update.md5[13],
					md5_update.md5[14], md5_update.md5[15]);

			if (xQueueSend(otaQueue, (void * ) &md5_update,
					(TickType_t ) 10) != pdPASS) {
				ESP_LOGI(TAG, "otaqueue post failure");
			}
		}
	}
}

static int handleControlMsg(esp_mqtt_event_handle_t event) {
	int ret = 0;
	if (event->topic_len > strlen(mqtt_sub_msg)) {
		const char* pTopic = &event->topic[strlen(mqtt_sub_msg) - 1];
		//check for control messages
		if (strcmp(pTopic, "control") == 0) {
			ESP_LOGI(TAG, "%.*s", event->topic_len - strlen(mqtt_sub_msg) + 1,
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

