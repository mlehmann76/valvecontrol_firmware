/*
 * status.c
 *
 *  Created on: 25.03.2019
 *      Author: marco
 */

#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "sdkconfig.h"
#include "esp_log.h"
#include "cJSON.h"

#include "mqtt_config.h"
#include "mqtt_client.h"
#include "mqtt_user.h"

#include "controlTask.h"

#include "status.h"

#define TAG "status"

EventGroupHandle_t status_event_group;

void addFirmwareStatus(cJSON *root);

typedef void (*pStatusFunc)(cJSON *root);

typedef struct {
	uint32_t bit;
	pStatusFunc pFunc;
} status_func_t;

status_func_t status_func[] = { //
		{ STATUS_EVENT_FIRMWARE, addFirmwareStatus },//
		{ STATUS_EVENT_CONTROL, addChannelStatus }};


void status_task_setup(void) {
	status_event_group = xEventGroupCreate();
	xTaskCreate(&status_task, "status_task", 2048, NULL, 5, NULL);
}

void status_task(void* pvParameters) {
	EventBits_t bits;
	while (1) {
		if ((status_event_group != NULL) && (mqtt_event_group != NULL)) {

			if ((xEventGroupGetBits(mqtt_event_group) & MQTT_CONNECTED_BIT) &&
					((bits = xEventGroupGetBits(status_event_group)) != 0)) {

				cJSON *pRoot = cJSON_CreateObject();
				if (pRoot != NULL) {
					for (size_t i = 0; i < (sizeof(status_func) / sizeof(status_func[0])); i++) {
						//at least one is set, so send all status
						if (status_func[i].pFunc != NULL) {
							status_func[i].pFunc(pRoot);
						}
					}

					xEventGroupClearBits(status_event_group, bits);

					char *string = cJSON_Print(pRoot);
					if (string == NULL) {
						ESP_LOGI(TAG, "Failed to print channel.");
						goto end;
					}

					message_t message = { .pTopic = (char*) getPubMsg(), .topic_len = 0, .pData = string, .data_len =
							strlen(string) };

					if ( xQueueSendToBack(pubQueue, &message, 10) != pdPASS) {
						// Failed to post the message, even after 10 ticks.
						ESP_LOGI(TAG, "pubqueue post failure");
					}

					end:

					cJSON_Delete(pRoot);
					/* reduce frequency by waiting some time*/
					vTaskDelay(500 / portTICK_PERIOD_MS);
				}
			}
		}
		vTaskDelay(10 / portTICK_PERIOD_MS);
	}
	vTaskDelete(NULL);
}

void addFirmwareStatus(cJSON *root) {
	if (root == NULL) {
		goto end;
	}

	cJSON *pcjsonfirm = cJSON_AddObjectToObject(root, "firmware");
	if (pcjsonfirm == NULL) {
		goto end;
	}

	if (cJSON_AddStringToObject(pcjsonfirm, "date", __DATE__) == NULL) {
		goto end;
	}

	if (cJSON_AddStringToObject(pcjsonfirm, "time", __TIME__) == NULL) {
		goto end;
	}

	end:

	return;
}

