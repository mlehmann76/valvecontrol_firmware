/*
 * status.c
 *
 *  Created on: 25.03.2019
 *      Author: marco
 */

#include <time.h>
#include <memory>
#include <chrono>
#include "QueueCPP.h"
#include "freertos/event_groups.h"
#include "SemaphoreCPP.h"
#include "TimerCPP.h"

#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_image_format.h"
#include "esp_ota_ops.h"
#include <esp_pthread.h>

#include "Json.h"

#include "config.h"
#include "mqtt_client.h"
#include "config_user.h"
#include "sht1x.h"

#include "statusTask.h"
#include "MainClass.h"

#define TAG "status"

void StatusTask::addTimeStamp(Json *root) {
	if (root == NULL) {
		return;
	}

	time_t now;
	struct tm timeinfo;
	time(&now);
	localtime_r(&now, &timeinfo);
	char strftime_buf[64];
	localtime_r(&now, &timeinfo);

	Json pcjsonfirm = root->addObject("datetime");
	strftime(strftime_buf, sizeof(strftime_buf), "%F", &timeinfo);
	pcjsonfirm.addItem("date", Json(strftime_buf));

	strftime(strftime_buf, sizeof(strftime_buf), "%T", &timeinfo);
	pcjsonfirm.addItem("time", Json(strftime_buf));

	return;
}

StatusTask::StatusTask(EventGroupHandle_t &main) :
		m_statusFunc(), main_event_group(&main), m_status(this) {
	addProvider(status());
	auto cfg = esp_pthread_get_default_config();
	cfg.thread_name = "status task";
	esp_pthread_set_cfg(&cfg);
	m_thread = std::thread([this]() {
		this->task();
	});
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

			for (auto _s : m_statusFunc) {
				if (_s->hasUpdate()) {
					needUpdate = true;
					break;
				}
			}

			if (((xEventGroupGetBits(*main_event_group) & (CONNECTED_BIT | MQTT_CONNECTED_BIT | MQTT_OTA_BIT))
					== (CONNECTED_BIT | MQTT_CONNECTED_BIT)) && needUpdate) {

				Json root;									//
				addTimeStamp(&root);
				//
				for (auto _s : m_statusFunc) {
					//at least one is set, so send all status
					if (_s->hasUpdate()) {
						_s->addStatus(&root);
						_s->setUpdate(false);
					}
				}

				mqtt::MqttQueueType message(new mqtt::mqttMessage(mqttConf.getPubMsg(), root.dump()));
				MainClass::instance()->mqtt().send(std::move(message));

				std::this_thread::sleep_for(std::chrono::milliseconds(500));

			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

void StatusTask::addProvider(StatusProviderBase &_stat) {
	m_statusFunc.push_back(&_stat);
}

void StatusTask::addFirmwareStatus(Json *root) {
	if (root == NULL) {
		return;
	}

	Json pcjsonfirm = root->addObject("firmware");

	pcjsonfirm.addItem("date", Json(__DATE__));
	pcjsonfirm.addItem("time", Json(__TIME__));
	pcjsonfirm.addItem("version", Json(PROJECT_GIT));
	pcjsonfirm.addItem("idf", Json(esp_get_idf_version()));
}

void StatusTask::addHardwareStatus(Json *root) {
	if (root == NULL) {
		return;
	}

	esp_chip_info_t chip_info;
	esp_chip_info(&chip_info);

	Json pcjsonfirm = root->addObject("hardware");
	pcjsonfirm.addItem("cores", Json((double)chip_info.cores));
	pcjsonfirm.addItem("rev", Json((double)chip_info.revision));

}
