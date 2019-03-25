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

void status_task_setup(void) {
	status_event_group = xEventGroupCreate();
	xTaskCreate(&status_task, "status_task", 2048, NULL, 5, NULL);
}

void status_task(void* pvParameters) {
	while (1) {
		if (status_event_group != NULL) {
			if (xEventGroupGetBits(status_event_group) != 0) {
				cJSON *pRoot = cJSON_CreateObject();

				if (pRoot != NULL) {

					if (xEventGroupClearBits(status_event_group, STATUS_EVENT_CONTROL) != 0) {
						addChannelStatus(pRoot);
					}

					char *string = cJSON_Print(pRoot);
					if (string == NULL) {
						ESP_LOGI(TAG, "Failed to print channel.");
						goto end;
					}

					message_t message = {
							.pTopic = (char*) getPubMsg(),
							.topic_len = 0,
							.pData = string,
							.data_len = strlen(string) };

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
