/*
 * mqtt_user_ota.c
 *
 *  Created on: 29.06.2018
 *      Author: marco
 */

#include <string.h>
#include <stdlib.h>

#include "sdkconfig.h"
#include "esp_log.h"

#include "mqtt_client.h"
#include "mqtt_user_ota.h"

#define TAG "OTA"

void handleOtaMessage(esp_mqtt_event_handle_t event) {
	ESP_LOGI(TAG,"handle ota message");
}
