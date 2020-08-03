/*
 * status.c
 *
 *  Created on: 25.03.2019
 *      Author: marco
 */

#include <time.h>
#include <memory>
#include "TaskCPP.h"
#include "QueueCPP.h"
#include "freertos/event_groups.h"
#include "SemaphoreCPP.h"
#include "TimerCPP.h"

#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_image_format.h"
#include "esp_ota_ops.h"
#include "cJSON.h"

#include "config.h"
#include "mqtt_client.h"
#include "config_user.h"
#include "sht1x.h"

#include "statusTask.h"
#include "MainClass.h"

#define TAG "status"

void StatusTask::addTimeStamp(cJSON *root) {
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

StatusTask::StatusTask(EventGroupHandle_t &main, mqtt::PubQueue &_queue) :
		TaskClass("status", TaskPrio_HMI, 3072), m_statusFunc(), m_statusFuncCount(0),
		main_event_group(&main), queue(_queue), m_status(this) {
	addProvider(status());
}

void StatusTask::task() {
	while (1) {
		if ((*main_event_group != NULL)) {
			bool needUpdate = false;

			if ((xEventGroupGetBits(*main_event_group) & CONNECTED_BIT) && !m_isConnected) {
				setUpdate(true);
				m_isConnected = true;
			}

			if (m_isConnected && !(xEventGroupGetBits(*main_event_group) & CONNECTED_BIT)) {
				m_isConnected = false;
			}

			for (size_t i = 0; i < (m_statusFuncCount); i++) {
				if (m_statusFunc[i]->hasUpdate()) {
					needUpdate = true;
					break;
				}
			}

			if (((xEventGroupGetBits(*main_event_group) & (CONNECTED_BIT | MQTT_CONNECTED_BIT | MQTT_OTA_BIT))
					== (CONNECTED_BIT | MQTT_CONNECTED_BIT))
					&& needUpdate) {


				cJSON *pRoot = cJSON_CreateObject();
				if (pRoot != NULL) {
					//
					addTimeStamp(pRoot);
					//
					for (size_t i = 0; i < (m_statusFuncCount); i++) {
						//at least one is set, so send all status
						if (m_statusFunc[i]->hasUpdate()) {
							m_statusFunc[i]->addStatus(pRoot);
							m_statusFunc[i]->setUpdate(false);
						}
					}

					std::unique_ptr<char[]> _s(cJSON_Print(pRoot));
					MainClass::instance()->mqtt().send({mqttConf.getPubMsg(),_s.get()});

					cJSON_Delete(pRoot);
					vTaskDelay(std::chrono::duration_cast<portTick>(std::chrono::milliseconds(500)).count());
				}
			}
		}
		vTaskDelay(10 / portTICK_PERIOD_MS);
	}
}

void StatusTask::addProvider(StatusProviderBase& _stat) {
	if (m_statusFuncCount < (sizeof(m_statusFunc)/sizeof(*m_statusFunc))) {
		m_statusFunc[m_statusFuncCount] = &_stat;
		m_statusFuncCount++;
	}
}

void StatusTask::addFirmwareStatus(cJSON *root) {
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

}

void StatusTask::addHardwareStatus(cJSON *root) {
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
}
