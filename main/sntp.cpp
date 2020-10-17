/*
 * sntp.c
 *
 *  Created on: 27.04.2019
 *      Author: marco
 */

#include "lwip/apps/sntp.h"
#include "config.h"
#include "config_user.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "repository.h"
#include "sdkconfig.h"
#include <string>
#include <sys/time.h>
#include <time.h>

static const char *TAG = "sntp";

void SntpSupport::init() {
    log_inst.info(TAG, "Initializing SNTP");
    server = Config::repo().get<std::string>("sntp", "server");
    timeZone = Config::repo().get<std::string>("sntp", "zone");
    log_inst.info(TAG, "sntp server ({}) ({})", server, timeZone);
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, server.c_str());
    // sntp_set_time_sync_notification_cb(time_sync_notification_cb);
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
    log_inst.info(TAG, "The current date/time is: {}", strftime_buf);
    return true;
}
