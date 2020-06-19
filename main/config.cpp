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
#include "cJSON_Utils.h"

#include "config.h"

static const char *TAG = "CONFIG";

char *nvs_json_config = NULL;

extern const char config_json_start[] asm("_binary_config_json_start");


namespace Config {

bool configBase::m_isInitialized = false;
const char MqttConfig::MQTT_PUB_MESSAGE_FORMAT[] = "%s%02X%02X%02X%02X%02X%02X%s";

configBase::configBase() {
	m_isInitialized = false;
}

esp_err_t configBase::init() {

	char *nvs_json_config;
	esp_err_t ret = ESP_OK;

	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		// NVS partition was truncated and needs to be erased
		// Retry nvs_flash_init
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}

	ESP_ERROR_CHECK(err);

	err = nvs_open("storage", NVS_READWRITE, &my_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "nvs_open storage failed (%s)", esp_err_to_name(err));
	}

	//try opening saved json config
	err = readStr(&my_handle, "config_json", &nvs_json_config);
	if (ESP_OK != err) {
		ESP_LOGE(TAG, "config_json read failed (%s)", esp_err_to_name(err));
		pConfig = cJSON_Parse(config_json_start);
	} else {
		pConfig = cJSON_Parse(config_json_start);
		cJSON *patch = NULL;
		patch = cJSON_Parse(nvs_json_config);
		pConfig = cJSONUtils_MergePatch(pConfig, patch);
		cJSON_Delete(patch);
	}
	//

	return ret;
}

configBase::~configBase() {
	// TODO Auto-generated destructor stub
}


esp_err_t MqttConfig::init() {

	if (!m_isInitialized) {
		configBase::init();
	}
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

	ESP_LOGI(TAG, "sub: (%s) pub (%s)", getSubMsg(), getPubMsg());

	return ESP_OK;
}

esp_err_t configBase::readConfigStr(const char *section, const char *name, char **dest) {
	char* ret = NULL;

	//	if (nvs_json_config != NULL) {
	//		cJSON *root = cJSON_Parse(nvs_json_config);
	//		ret = readJsonConfigStr(root, "nvs", section, name);
	//		cJSON_Delete(root);
	//	}

		if (ret == NULL) {
			//cJSON *root = cJSON_Parse(config_json_start);
			ret = readJsonConfigStr(pConfig, "default", section, name);
			//cJSON_Delete(root);
		}

		*dest = ret;
		return ret != NULL ? ESP_OK : !ESP_OK;
}

esp_err_t configBase::readStr(nvs_handle *pHandle, const char *pName, char **dest) {
	size_t required_size;
	char * temp;
	esp_err_t err = nvs_get_str(*pHandle, pName, NULL, &required_size);
	if (err == ESP_OK) {
		temp = (char*)malloc(required_size);
		if (temp != NULL) {
			nvs_get_str(*pHandle, pName, temp, &required_size);
			*dest = temp;
		} else {
			err = ESP_ERR_NO_MEM;
		}
	}
	return err;
}

esp_err_t configBase::writeStr(nvs_handle *pHandle, const char *pName, const char *str) {
	return nvs_set_str(*pHandle, pName, str);
}

char* configBase::readJsonConfigStr(const cJSON *pRoot, const char *cfg, const char* section, const char* name) {
	char* ret = NULL;
	cJSON* pSection = cJSON_GetObjectItem(pRoot, section);
	if (pSection != NULL) {
		cJSON* pName = cJSON_GetObjectItem(pSection, name);

		if (cJSON_IsString(pName) && (pName->valuestring != NULL)) {
			ret = (char*)malloc(strlen(pName->valuestring));
			strcpy(ret, pName->valuestring);
			ESP_LOGI(TAG, "section (%s), name (%s) in (%s) found (%s)", section, name, cfg, ret);
		} else {
			ESP_LOGE(TAG, "name in (%s) not found (%s)", cfg, name);
		}
	} else {
		ESP_LOGE(TAG, "section in (%s) not found (%s)", cfg, section);
	}
	return ret;
}
char* MqttConfig::stringify() {
	return NULL;
}

void MqttConfig::parse(const char*) {
}

esp_err_t SysConfig::init() {
	return ESP_OK;
}

char* SysConfig::stringify() {
	return NULL;
}

void SysConfig::parse(const char*) {
}

} /* namespace Config */

//Globals
Config::MqttConfig mqtt;
Config::SysConfig sys;
