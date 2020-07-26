/*
 * MainClass.cpp
 *
 *  Created on: 21.07.2020
 *      Author: marco
 */

#include <string.h>
#include <stdlib.h>

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
#include "messager.h"
#include "sntp.h"
#include "channelFactory.h"
#include "channelAdapter.h"

#include "MainClass.h"

#define TAG "MAIN"

MainClass::MainClass() {
	// TODO Auto-generated constructor stub

}

MainClass::~MainClass() {
	// TODO Auto-generated destructor stub
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
	esp_log_level_set("phy_init", ESP_LOG_ERROR);
	esp_log_level_set("wifi", ESP_LOG_ERROR);

	spiffsInit();

	mqttConf.init();
	chanConf.init();
	sensorConf.init();
	sntp_support();

	mqttUser.init();
	//mqttConf.setNext(&sysConf)->setNext(&chanConf)->setNext(&sensorConf);
	//messager.addHandler("/config", &mqttConf);

	std::vector<AbstractChannel*> _channels(4);

	for (size_t i=0; i< _channels.size();i++) {
		_channels[i] = LedcChannelFactory::channel(i, 1800);
		_channels[i]->add(new MqttChannelAdapter(messager,std::string(mqttConf.getDevName()) + "channel" + std::to_string(i)));
		_channels[i]->add(new MqttJsonChannelAdapter(messager,std::string(mqttConf.getDevName()) + "control"));
	}

	ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");

	ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());

	size_t heapFree = esp_get_free_heap_size();
	time_t now, lastMqttSeen;
	struct tm timeinfo;
	time(&lastMqttSeen);
	int count=0;

	while (1) {
		time(&now);
		localtime_r(&now, &timeinfo);
		if (timeinfo.tm_year > (2016 - 1900)) {
			//reboot @0:0:0 if received sntp
			if ((timeinfo.tm_hour == 0) && (timeinfo.tm_min == 0)) {
				if ((timeinfo.tm_sec == 0) || (timeinfo.tm_sec == 1)) {
					esp_restart();
				}
			}
		}
		if (0 == count) {
			if( esp_get_free_heap_size() != heapFree) {
				heapFree = esp_get_free_heap_size();
				ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
				count = 500;
			}
		} else {
			count--;
		}
		vTaskDelay(1);
	}
}
