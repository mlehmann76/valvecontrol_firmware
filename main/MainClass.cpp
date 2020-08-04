/*
 * MainClass.cpp
 *
 *  Created on: 21.07.2020
 *      Author: marco
 */

#include <string.h>
#include <stdlib.h>
#include <memory>

#include "sdkconfig.h"

#include "TaskCPP.h"
#include "SemaphoreCPP.h"
#include "QueueCPP.h"
#include "TimerCPP.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wps.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "esp_task_wdt.h"
#include "esp_http_server.h"
#include "mqtt_client.h"
#include "cJSON.h"

#include "config.h"
#include "config_user.h"
#include "sntp.h"
#include "channelFactory.h"
#include "channelAdapter.h"
#include "otaHandler.h"

#include "MainClass.h"

#include "echoServer.h"
#define TAG "MAIN"

template<typename ... Args>
std::string string_format(const std::string &format, Args ... args) {
	size_t size = snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
	if (size > 0) {
		std::unique_ptr<char[]> buf(new char[size]);
		snprintf(buf.get(), size, format.c_str(), args ...);
		return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
	} else {
		return std::string("");
	}
}

void MainClass::spiffsInit(void) {
	ESP_LOGI(TAG, "Initializing SPIFFS");

	esp_vfs_spiffs_conf_t conf = { .base_path = "/spiffs", .partition_label = NULL, .max_files = 5,
			.format_if_mount_failed = false };

	// Use settings defined above to initialize and mount SPIFFS filesystem.
	// Note: esp_vfs_spiffs_register is an all-in-one convenience function.
	esp_err_t ret = esp_vfs_spiffs_register(&conf);

	if (ret != ESP_OK) {
		if (ret == ESP_FAIL) {
			ESP_LOGE(TAG, "Failed to mount or format filesystem");
		} else if (ret == ESP_ERR_NOT_FOUND) {
			ESP_LOGE(TAG, "Failed to find SPIFFS partition");
		} else {
			ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
		}
		return;
	}

	size_t total = 0, used = 0;
	ret = esp_spiffs_info(NULL, &total, &used);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
	} else {
		ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
	}
}

int MainClass::loop() {
	esp_log_level_set("*", ESP_LOG_VERBOSE);
	esp_log_level_set("ECHO", ESP_LOG_VERBOSE);
	esp_log_level_set("SOCKET", ESP_LOG_VERBOSE);
	esp_log_level_set("MQTTS", ESP_LOG_VERBOSE);

	spiffsInit();

	mqttConf.init();
	chanConf.init();
	sensorConf.init();
	sntp_support();

	mqttUser.init();
	wifitask.addConnectionObserver(mqttUser.obs());
	EchoServer echo;
	wifitask.addConnectionObserver(echo.obs());

	//mqttConf.setNext(&sysConf)->setNext(&chanConf)->setNext(&sensorConf);
	MqttOtaHandler mqttOta(otaWorker, mqttUser,
			string_format("%sota", mqttConf.getDevName()),
			string_format("%sota/$implementation/binary", mqttConf.getDevName()));

	std::vector<ChannelBase*> _channels(4);
	ExclusiveAdapter cex; //only one channel should be active

	for (size_t i=0; i< _channels.size();i++) {
		_channels[i] = LedcChannelFactory::channel(i, std::chrono::seconds(chanConf.getTime(i)));
		_channels[i]->add(&cex);
		_channels[i]->add(new MqttChannelAdapter(mqttUser,
				string_format("%schannel%d/control", mqttConf.getDevName(),i),
				string_format("%schannel%d/state", mqttConf.getDevName(),i)));
		_channels[i]->add(new MqttJsonChannelAdapter(mqttUser,
				string_format("%scontrol", mqttConf.getDevName()),
				string_format("%sstate", mqttConf.getDevName())));
	}

	int count = 0;
	uint32_t heapFree = 0;
	std::unique_ptr<char[]> pcWriteBuffer(new char[2048]);

	while (1) {
		//check for time update by sntp
		if (!(xEventGroupGetBits(MainClass::instance()->eventGroup()) & SNTP_UPDATED) && (update_time() != ESP_ERR_NOT_FOUND)) {
			xEventGroupSetBits(MainClass::instance()->eventGroup(),SNTP_UPDATED);
			status.setUpdate(true);
		}

		if (0 == count) {
			if( esp_get_free_heap_size() != heapFree) {
				heapFree = esp_get_free_heap_size();
				vTaskGetRunTimeStats(pcWriteBuffer.get());
				ESP_LOGI(TAG, "[APP] Free memory: %d bytes\n%s", esp_get_free_heap_size(),pcWriteBuffer.get());
				count = 500;
			}
		} else {
			count--;
		}
		vTaskDelay(1);
	}
}
