/*
 * ChannelAdapter.cpp
 *
 *  Created on: 23.07.2020
 *      Author: marco
 */

#include <stdio.h>
#include <time.h>
#include "esp_log.h"

#include "sdkconfig.h"
#include "config.h"
#include "config_user.h"

#include "channelAdapter.h"

#define TAG "channelAdapter"

int MqttChannelAdapter::onMessage(esp_mqtt_event_handle_t event) {
	if (event->data_len < 64) {
		ESP_LOGD(TAG, "Topic received!: (%d) %.*s (%d) %.*s", event->topic_len, event->topic_len, event->topic,
				event->data_len, event->data_len, event->data);
	} else {
		ESP_LOGD(TAG, "Topic received!: (%d) %.*s", event->topic_len, event->topic_len, event->topic);
	}
	//check message and set channel or notify
	return 1;
}
