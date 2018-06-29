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

#include "mqtt_client.h"
#include "mqtt_user.h"
#include "mqtt_user_ota.h"
#include "gpioTask.h"


static const char *TAG = "MQTTS";
static const char* chanNames[] = {"channel0","channel1","channel2","channel3"};
static const int maxChanIndex = sizeof(chanNames)/sizeof(chanNames[0]);

static char mqtt_sub_msg[64] = {0};
static char mqtt_pub_msg[64] = {0};

esp_mqtt_client_handle_t client = NULL;

static void mqtt_message_handler(esp_mqtt_event_handle_t event) ;

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
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
			ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            mqtt_message_handler(event);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
    }
    return ESP_OK;
}


static void handleChannelControl(int chan, esp_mqtt_event_handle_t event) {

	gpio_task_mode_t func = mStatus;

	if (event->data_len == 0) {
		func = mStatus;
	} else if (strncmp((const char*) event->data, "on", event->data_len) == 0) {
		func = mOn;
	} else if (strncmp((const char*) event->data, "off", event->data_len)
			== 0) {
		func = mOff;
	} else {
		chan = -1;
	}

	if (chan != -1) {
		queueData_t data = { chan, func };
		// available if necessary.
		if (xQueueSend(subQueue, (void * ) &data,
				(TickType_t ) 10) != pdPASS) {
			// Failed to post the message, even after 10 ticks.
			ESP_LOGI(TAG, "subqueue post failure");
		}
	}
}

static void mqtt_message_handler(esp_mqtt_event_handle_t event) {

	const char *pTopic;
	int done = 0;

	ESP_LOGI(TAG, "Topic received!: %.*s %.*s", event->topic_len,
			 event->topic, event->data_len,	event->data);

	if (event->topic_len > strlen(mqtt_sub_msg)) {
		pTopic = &event->topic[strlen(mqtt_sub_msg)-1];

		ESP_LOGI(TAG, "%.*s", event->topic_len - strlen(mqtt_sub_msg) + 1, pTopic);

		if (*pTopic != 0) {
			int chan = -1;
			char buf[128];
			for (int i = 0; i < maxChanIndex; i++) {
				/* add /control/channelName to check for messages */
				snprintf(buf, sizeof(buf), "%s%s", "/control/", chanNames[i]);

				if (strncmp(pTopic, buf, event->topic_len - strlen(mqtt_sub_msg) + 1) == 0) {
					chan = i;
					ESP_LOGI(TAG,"channel :%d found", i);
					handleChannelControl(chan, event);
					done = 1;
					break;
				}
			}

			if (!done && strncmp(pTopic, "ota", strlen("ota")) == 0) {
				handleOtaMessage(event);
			}
		}
	}
}


void mqtt_task(void *pvParameters) {
	int msg_id;
	while (1) {
		// check if we have something to do
		queueData_t rxData = { 0 };

		if ((client != NULL)
				&& (xQueueReceive(pubQueue, &(rxData), (TickType_t ) 10))) {

			ESP_LOGI(TAG, "pubQueue work");

			int chan = rxData.chan;

			if (chan != -1 && (chan < maxChanIndex)) {
				char buf[255] = { 0 };
				char payload[16] = { 0 };

				snprintf(buf, sizeof(buf), "%s%s", mqtt_pub_msg, chanNames[chan]);
				snprintf(payload, sizeof(payload), "%s",
						rxData.mode == mOn ? "on" : "off");

				ESP_LOGI(TAG, "publish %.*s : %.*s", strlen(buf), buf,
						strlen(payload), payload);

				msg_id = esp_mqtt_client_publish(client, buf, payload,
						strlen(payload) + 1, 1, 0);
				ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

			}
		}
		vTaskDelay(10);
		taskYIELD();
	}
	vTaskDelete(NULL);
}

static void mqtt_app_start(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .uri = MQTT_SERVER, .event_handle = mqtt_event_handler,
        // .user_context = (void *)your_context
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);
}

void mqtt_user_init(void) {
	uint8_t mac[6] = {0};
	esp_efuse_mac_get_default(mac);
	snprintf(mqtt_sub_msg, sizeof(mqtt_sub_msg), MQTT_PUB_MESSAGE_FORMAT, mac[5],mac[4],"#");
	snprintf(mqtt_pub_msg, sizeof(mqtt_pub_msg), MQTT_PUB_MESSAGE_FORMAT, mac[5],mac[4], "state/");

	ESP_LOGI(TAG, "sub: %s, pub: %s", mqtt_sub_msg, mqtt_pub_msg);

	xTaskCreatePinnedToCore(&mqtt_task, "mqtt_task", 2*8192, NULL, 5, NULL, 0);

	mqtt_app_start();
}


