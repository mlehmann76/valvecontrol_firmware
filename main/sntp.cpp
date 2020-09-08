/*
 * sntp.c
 *
 *  Created on: 27.04.2019
 *      Author: marco
 */

#include <string>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/apps/sntp.h"
#include "sdkconfig.h"
#include "config.h"
#include "repository.h"

static const char *TAG = "sntp";

void SntpSupport::init() {
	ESP_LOGI(TAG, "Initializing SNTP");
	server = Config::repo().get<std::string>("sntp","server");
	timeZone = Config::repo().get<std::string>("sntp","zone");
	ESP_LOGI(TAG, "sntp server (%s) (%s)", server.c_str(), timeZone.c_str());
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_setservername(0, server.c_str());
	//sntp_set_time_sync_notification_cb(time_sync_notification_cb);
	sntp_init();
	setenv("TZ", timeZone.c_str(), 1);
	tzset();
}

bool SntpSupport::update() {
	time_t now;
	struct tm timeinfo;
	char strftime_buf[64];

	time(&now);
	localtime_r(&now, &timeinfo);
	// Is time set? If not, tm_year will be (1970 - 1900).

	if (timeinfo.tm_year < (2016 - 1900)) {
		return false;
	}
	// update 'now' variable with current time
	time(&now);
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	ESP_LOGI(TAG, "The current date/time is: %s", strftime_buf);
	return true;
}
