/*
 * config.cpp
 *
 *  Created on: 19.06.2020
 *      Author: marco
 */
#include <string.h>
#include <stdlib.h>

#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "cJSON.h"
#include "mqtt_config.h"

#include "jsonconfig.h"
#include "config.h"

static const char *TAG = "CONFIG";

namespace Config {

configBase::configBase() {
	// TODO Auto-generated constructor stub

}

configBase::~configBase() {
	// TODO Auto-generated destructor stub
}

const char MqttConfig::MQTT_PUB_MESSAGE_FORMAT[] = "%s%02X%02X%02X%02X%02X%02X%s";

void MqttConfig::init() {

	size_t required_size = 0;
	/* read mqtt device name */

	if (ESP_OK == readConfigStr("mqtt", "device", &mqtt_device_name)) {
		required_size = strlen(mqtt_device_name);//TODO strlen is risky here
	}else{
		/* set default device name */
		uint8_t mac[6] = { 0 };
		esp_efuse_mac_get_default(mac);
		snprintf(def_mqtt_device, sizeof(def_mqtt_device), MQTT_PUB_MESSAGE_FORMAT,
				MQTT_DEVICE, mac[0], mac[1],mac[2], mac[3],mac[4], mac[5],"/");
		mqtt_device_name = def_mqtt_device;
		required_size = strlen(def_mqtt_device);//TODO strlen is risky here
	}

	mqtt_sub_msg = (char*)malloc(required_size+sizeof("#")+1);
	mqtt_pub_msg = (char*)malloc(required_size+sizeof("state/")+1);

	if ((mqtt_sub_msg != NULL)&&(mqtt_pub_msg!=NULL)) {
		snprintf(mqtt_sub_msg, required_size+sizeof("#")+1, "%s#", mqtt_device_name);
		snprintf(mqtt_pub_msg, required_size+sizeof("state/")+1, "%sstate/", mqtt_device_name);
	}

	/* read mqtt server name */
	ESP_ERROR_CHECK(readConfigStr("mqtt", "server", &mqtt_server));

	/* read mqtt user name */
	ESP_ERROR_CHECK(readConfigStr("mqtt", "user", &mqtt_user));

	/* read mqtt user pass */
	ESP_ERROR_CHECK(readConfigStr("mqtt", "pass", &mqtt_pass));

	ESP_LOGI(TAG, "sub: (%s) pub (%s)", getSubMsg(),getPubMsg());
}

} /* namespace Config */

//Globals
Config::MqttConfig mqtt;
