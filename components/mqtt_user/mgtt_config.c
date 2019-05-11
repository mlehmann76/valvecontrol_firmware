/*
 * mgtt_config.c
 *
 *  Created on: 27.10.2018
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

static const char *TAG = "MQTT_CNF";

static char def_mqtt_device[64] = { 0 }; //TODO
static char *mqtt_server = (char*)MQTT_SERVER;
static char *mqtt_sub_msg = NULL;
static char *mqtt_pub_msg = NULL;
static char *mqtt_device_name = NULL;
static char *mqtt_user = (char*)MQTT_USER;
static char *mqtt_pass = (char*)MQTT_PASS;



const char* getSubMsg() {
	return mqtt_sub_msg;
}

const char* getPubMsg() {
	return mqtt_pub_msg;
}

const char* getDevName() {
	return mqtt_device_name;
}

const char* getMqttServer() {
	return mqtt_server;
}

const char* getMqttUser() {
	return mqtt_user;
}

const char* getMqttPass() {
	return mqtt_pass;
}

/*
esp_err_t setMqttConfig(cJSON* config) {
	char* pHost = cJSON_GetStringValue(cJSON_GetObjectItem(config, "host"));
	cJSON* pPort = cJSON_GetObjectItem(config, "port");

	if ((pHost != NULL) && (pPort != NULL) && cJSON_IsNumber(pPort)) {
		ESP_LOGI(TAG, "config found host %s:%d", pHost, pPort->valueint);
	}

	char* pUser = cJSON_GetStringValue(cJSON_GetObjectItem(config, "user"));
	char* pPass  = cJSON_GetStringValue(cJSON_GetObjectItem(config, "pass"));
	if ((pUser != NULL) && (pPass != NULL)) {
		ESP_LOGI(TAG, "user/pass found %s:%s", pUser, pPass);
		//FIXME writeStr(&my_handle, "mqtt_user", pUser);
		//FIXME writeStr(&my_handle, "mqtt_pass", pPass);
	}

	char* pDev   = cJSON_GetStringValue(cJSON_GetObjectItem(config, "device"));
	if ((pDev != NULL)) {
		ESP_LOGI(TAG, "device found %s", pDev);
		//FIXME writeStr(&my_handle, "mqtt_device", pDev);
	}

	return ESP_OK;
}
*/

void mqtt_config_init() {
	// Initialize NVS
//	esp_err_t err = nvs_flash_init();
//	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
//		// NVS partition was truncated and needs to be erased
//		// Retry nvs_flash_init
//		ESP_ERROR_CHECK(nvs_flash_erase());
//		err = nvs_flash_init();
//	}
//
//	ESP_ERROR_CHECK(err);
//
//	err = nvs_open("storage", NVS_READWRITE, &my_handle);
//	if (err != ESP_OK) {
//		ESP_LOGE(TAG, "nvs_open storage failed (%s)", esp_err_to_name(err));
//	}

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

	mqtt_sub_msg = malloc(required_size+sizeof("#")+1);
	mqtt_pub_msg = malloc(required_size+sizeof("state/")+1);

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

