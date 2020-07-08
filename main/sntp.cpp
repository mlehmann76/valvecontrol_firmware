/*
 * sntp.c
 *
 *  Created on: 27.04.2019
 *      Author: marco
 */

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/apps/sntp.h"
#include "sdkconfig.h"
#include "config.h"

static const char *TAG = "sntp";
char* server = NULL;
char* timeZone = NULL;

esp_err_t update_time(void) {
	time_t now;
	struct tm timeinfo;
	char strftime_buf[64];

	time(&now);
	localtime_r(&now, &timeinfo);
	// Is time set? If not, tm_year will be (1970 - 1900).

	if (timeinfo.tm_year < (2016 - 1900)) {
		return ESP_ERR_NOT_FOUND;
	}
	// update 'now' variable with current time
	time(&now);
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	ESP_LOGI(TAG, "The current date/time is: %s", strftime_buf);
	return ESP_OK;
}

void sntp_support(void) {
	ESP_LOGI(TAG, "Initializing SNTP");
	//FIXME free memory before reallocate
	server = const_cast<char*>(sysConf.getTimeServer());
	timeZone = const_cast<char*>(sysConf.getTimeZone());
	if (server && timeZone) {
		ESP_LOGI(TAG, "sntp server (%s) (%s)", server, timeZone);
		sntp_setoperatingmode(SNTP_OPMODE_POLL);
		sntp_setservername(0, server);
		//sntp_set_time_sync_notification_cb(time_sync_notification_cb);
		sntp_init();
		setenv("TZ", timeZone, 1);
		tzset();
	}
}
