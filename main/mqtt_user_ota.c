/*
 * mqtt_user_ota.c
 *
 *  Created on: 29.06.2018
 *      Author: marco
 */

#include <string.h>
#include <stdlib.h>

#include "sdkconfig.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "mqtt_client.h"
#include "mqtt_user_ota.h"

#define TAG "OTA"

extern esp_mqtt_client_handle_t client;
extern char mqtt_pub_msg[64];
char payload[2048];
int payload_len = 0;

void handleOtaMessage(esp_mqtt_event_handle_t event) {
	ESP_LOGI(TAG, "handle ota message, len %d", event->data_len);

	//payload = malloc(event->data_len);
	//if (event->data_len < sizeof(payload)) {
		memcpy(payload, event->data, strlen("counter 00000"));
		payload_len = event->data_len;
	//}
}

void handleOta() {
	if (client != NULL && payload_len != 0) {
		char buf[255] = { 0 };

		snprintf(buf, sizeof(buf), "%s%s", mqtt_pub_msg, "ota/");

		ESP_LOGI(TAG, "publish %.*s ", strlen(buf), buf);

		//int msg_id = esp_mqtt_client_publish(client, buf, payload, payload_len,	0, 0);
		esp_mqtt_client_publish(client, buf, payload, strlen("counter 00000"),	0, 0);

		//ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

		//free(payload);
		payload_len = 0;
		//payload = NULL;
	}
}

void mqtt_ota_task(void *pvParameters) {
	for (;;) {
		handleOta();
		vTaskDelay(1);
		taskYIELD();
	}
}
