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
#include "freertos/event_groups.h"
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
#include "channel.h"
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
	channel.setup();

	status.addProvider(channel.status());
	status.addProvider(sht1x.status());

	mqttUser.init();
	//mqttConf.setNext(&sysConf)->setNext(&chanConf)->setNext(&sensorConf);
	//messager.addHandler("/config", &mqttConf);

	LedcChannelFactory fact;

	Channel channel0 = {fact.channel(0, 1800)};
	channel0.add(new MqttChannelAdapter(messager,std::string(mqttConf.getSubMsg()) + "/channel0"));

	ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
	//xTaskCreate(wifi_init_sta, "wifi init task", 4096, &server, 10, NULL);

	ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());

	size_t heapFree = esp_get_free_heap_size();
	time_t now, lastMqttSeen;
	struct tm timeinfo;
	time(&lastMqttSeen);

	while (1) {

#if 0
			char strftime_buf[64];
			time(&now);
			localtime_r(&now, &timeinfo);
			//if (xEventGroupWaitBits(button_event_group, WPS_SHORT_BIT, true, true,10)) {
			/*
			 time(&now);
			 if (difftime(now,last) >=2 ) {
			 vTaskGetRunTimeStats(task_debug_buf);
			 ESP_LOGI(TAG, "%s", task_debug_buf);
			 last = now;
			 }
			 */
			if (timeinfo.tm_year > (2016 - 1900)) {
				localtime_r(&now, &timeinfo);
				strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
				ESP_LOGI(TAG, "The current date/time is: %s", strftime_buf);
				vTaskDelay(5000 / portTICK_PERIOD_MS);
			} else {
				vTaskDelay(500 / portTICK_PERIOD_MS);
			}
	#else
		time(&now);
		localtime_r(&now, &timeinfo);
		if (timeinfo.tm_year > (2016 - 1900)) {
			//reboot @0:0:0 if received sntp
			if ((timeinfo.tm_hour == 0) && (timeinfo.tm_min == 0)) {
				if ((timeinfo.tm_sec == 0) || (timeinfo.tm_sec == 1)) {
					esp_restart();
				}
			}
		} else { // reboot after 1h when not connected to mqtt server
			if (difftime(now, lastMqttSeen) > 3600) {
				esp_restart();
			}
		}
		if (esp_get_free_heap_size() != heapFree) {
			heapFree = esp_get_free_heap_size();
			ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
		}
		vTaskDelay(1);
#endif
	}
}
