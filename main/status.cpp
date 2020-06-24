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
#include "freertos/semphr.h"

#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_image_format.h"
#include "esp_ota_ops.h"
#include "cJSON.h"

#include "config.h"
#include "mqtt_client.h"
#include "mqtt_user.h"

#include "controlTask.h"

#include "config_user.h"
#include "status.h"
#include "sht1x.h"

#define TAG "status"

EventGroupHandle_t status_event_group;

void addFirmwareStatus(cJSON *root);
void addHardwareStatus(cJSON *root);
void addTimeStamp(cJSON *root);

typedef void (*pStatusFunc)(cJSON *root);

typedef struct {
	uint32_t bit;
	pStatusFunc pFunc;
} status_func_t;

status_func_t status_func[] = { //
		{ STATUS_EVENT_FIRMWARE, addFirmwareStatus },//
		{ STATUS_EVENT_HARDWARE, addHardwareStatus },//
		{ STATUS_EVENT_CONTROL, addChannelStatus },
		{ STATUS_EVENT_SENSOR, addSHT1xStatus }};


void status_task_setup(void) {
	status_event_group = xEventGroupCreate();
	xTaskCreate(&status_task, "status_task", 2048, NULL, 5, NULL);
}

void status_task(void* pvParameters) {
	EventBits_t bits;
	while (1) {
		if ((status_event_group != NULL) && (main_event_group != NULL)) {

			if (((xEventGroupGetBits(main_event_group) & (CONNECTED_BIT | MQTT_CONNECTED_BIT | MQTT_OTA_BIT))
					== (CONNECTED_BIT | MQTT_CONNECTED_BIT))
					&& ((bits = xEventGroupGetBits(status_event_group)) != 0)) {

				cJSON *pRoot = cJSON_CreateObject();
				if (pRoot != NULL) {
					//
					addTimeStamp(pRoot);
					//
					for (size_t i = 0; i < (sizeof(status_func) / sizeof(status_func[0])); i++) {
						//at least one is set, so send all status
						if (status_func[i].pFunc != NULL && (bits & status_func[i].bit)) {
							status_func[i].pFunc(pRoot);
						}
					}

					xEventGroupClearBits(status_event_group, bits);

					char *string = cJSON_Print(pRoot);
					if (string == NULL) {
						ESP_LOGI(TAG, "Failed to print channel.");
						return;
					}

					message_t message = { .pTopic = (char*) mqtt.getPubMsg(), .pData = string, .topic_len = 0, .data_len =
							strlen(string) };

					if ( xQueueSendToBack(pubQueue, &message, 10) != pdPASS) {
						// Failed to post the message, even after 10 ticks.
						ESP_LOGI(TAG, "pubqueue post failure");
						free(string);
					}


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
		return;
	}

	cJSON *pcjsonfirm = cJSON_AddObjectToObject(root, "firmware");
	if (pcjsonfirm == NULL) {
		return;
	}
#if (1)
	if (cJSON_AddStringToObject(pcjsonfirm, "date", __DATE__) == NULL) {
		return;
	}

	if (cJSON_AddStringToObject(pcjsonfirm, "time", __TIME__) == NULL) {
		return;
	}
	if (cJSON_AddStringToObject(pcjsonfirm, "version", PROJECT_GIT) == NULL) {
		return;
	}
	if (cJSON_AddStringToObject(pcjsonfirm, "idf", esp_get_idf_version()) == NULL) {
		return;
	}

#else
	if (cJSON_AddStringToObject(pcjsonfirm, "name", esp_ota_get_app_description()->project_name) == NULL) {
		return;
	}

	if (cJSON_AddStringToObject(pcjsonfirm, "version", esp_ota_get_app_description()->version) == NULL) {
		return;
	}

	if (cJSON_AddStringToObject(pcjsonfirm, "date", esp_ota_get_app_description()->date) == NULL) {
		return;
	}

	if (cJSON_AddStringToObject(pcjsonfirm, "time", esp_ota_get_app_description()->time) == NULL) {
		return;
	}
#endif

	return;
}

void addHardwareStatus(cJSON *root) {
	if (root == NULL) {
		return;
	}

	esp_chip_info_t chip_info;
	esp_chip_info(&chip_info);

	cJSON *pcjsonfirm = cJSON_AddObjectToObject(root, "hardware");
	if (pcjsonfirm == NULL) {
		return;
	}
	if (cJSON_AddNumberToObject(pcjsonfirm, "cores", chip_info.cores) == NULL) {
		return;
	}

	if (cJSON_AddNumberToObject(pcjsonfirm, "rev", chip_info.revision) == NULL) {
		return;
	}

	return;
}

void addTimeStamp(cJSON *root) {
	if (root == NULL) {
		return;
	}

	time_t now;
	struct tm timeinfo;
	time(&now);
	localtime_r(&now, &timeinfo);
	char strftime_buf[64];
	localtime_r(&now, &timeinfo);


	cJSON *pcjsonfirm = cJSON_AddObjectToObject(root, "datetime");
	if (pcjsonfirm == NULL) {
		return;
	}

	strftime(strftime_buf, sizeof(strftime_buf), "%F", &timeinfo);
	if (cJSON_AddStringToObject(pcjsonfirm, "date", strftime_buf) == NULL) {
		return;
	}

	strftime(strftime_buf, sizeof(strftime_buf), "%T", &timeinfo);
	if (cJSON_AddStringToObject(pcjsonfirm, "time", strftime_buf) == NULL) {
		return;
	}

	return;
}
