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

#include "mqtt_config.h"

static const char *TAG = "MQTT_CNF";

char def_mqtt_sub_msg[64] = { 0 }; //TODO
char def_mqtt_pub_msg[64] = { 0 }; //TODO
char *mqtt_server = NULL;
char *mqtt_sub_msg = NULL;
char *mqtt_pub_msg = NULL;
char *mqtt_device_name = NULL;

const char* getSubMsg() {
	if (mqtt_sub_msg != NULL) {
		return mqtt_sub_msg;
	}else{
		return def_mqtt_sub_msg;
	}
}

const char* getPubMsg() {
	if (mqtt_pub_msg != NULL) {
		return mqtt_pub_msg;
	}else{
		return def_mqtt_pub_msg;
	}
}

const char* getMqttServer() {
	if (mqtt_server == NULL) {
		return MQTT_SERVER;
	} else {
		return mqtt_server;
	}
}

void mqtt_config_init() {
	// Initialize NVS
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		// NVS partition was truncated and needs to be erased
		// Retry nvs_flash_init
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}

	ESP_ERROR_CHECK(err);

	nvs_handle my_handle;
	err = nvs_open("storage", NVS_READWRITE, &my_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "nvs_open storage failed (%s)", esp_err_to_name(err));
	}

	/* read mqtt server name */
	size_t required_size;
	err = nvs_get_str(my_handle, "server_name", NULL, &required_size);
	if (err == ESP_OK) {
		mqtt_server = malloc(required_size);
		nvs_get_str(my_handle, "server_name", mqtt_server, &required_size);
	}

	uint8_t mac[6] = { 0 };
	esp_efuse_mac_get_default(mac);

	/* read mqtt device name */
	err = nvs_get_str(my_handle, "mqtt_device_name", NULL, &required_size);
	if (err == ESP_OK) {
		mqtt_device_name = malloc(required_size);
		nvs_get_str(my_handle, "mqtt_device_name", mqtt_device_name, &required_size);
		mqtt_sub_msg = malloc(required_size+sizeof("/")+1);
		mqtt_pub_msg = malloc(required_size+sizeof("/state/")+1);

		if ((mqtt_sub_msg != NULL)&&(mqtt_pub_msg!=NULL)) {
			snprintf(mqtt_sub_msg, required_size+sizeof("/")+1, "%s/#", mqtt_device_name);
			snprintf(mqtt_pub_msg, required_size+sizeof("/state/")+1, "%s/state/", mqtt_device_name);
		}
	}

	snprintf(def_mqtt_sub_msg, sizeof(def_mqtt_sub_msg), MQTT_PUB_MESSAGE_FORMAT,
			mac[0], mac[1],mac[2], mac[3],mac[4], mac[5], "#");

	snprintf(def_mqtt_pub_msg, sizeof(def_mqtt_pub_msg), MQTT_PUB_MESSAGE_FORMAT,
			mac[0], mac[1],mac[2], mac[3],mac[4], mac[5], "state/");

	ESP_LOGI(TAG, "sub: (%s) pub (%s)", getSubMsg(),getPubMsg());
}

