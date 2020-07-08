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

extern const char config_json_start[] asm("_binary_config_json_start");

namespace Config {

bool configBase::m_isInitialized = false;
cJSON *configBase::pConfig = NULL;
const char MqttConfig::MQTT_PUB_MESSAGE_FORMAT[] = "%s%02X%02X%02X%02X%02X%02X%s";

configBase::configBase(const char *_name) :
		ParseHandler(_name), m_string(), my_handle() {
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
	m_isInitialized = true;

	return ret;
}

esp_err_t configBase::readStr(nvs_handle *pHandle, const char *pName, char **dest) {
	size_t required_size;
	char *temp;
	esp_err_t err = nvs_get_str(*pHandle, pName, NULL, &required_size);
	if (err == ESP_OK) {
		temp = (char*) malloc(required_size);
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

esp_err_t configBase::readConfig(const char *section, const char *name, char **dest) {
	char *ret = NULL;
	if (!m_isInitialized) {
		init();
	}
	ret = readJsonConfigStr(pConfig, "default", section, name);

	*dest = ret;
	return ret != NULL ? ESP_OK : !ESP_OK;
}

char* configBase::readString(const char *section, const char *name) {
	char *ret;
	readConfig(section, name, &ret);
	return ret;
}

char* configBase::readJsonConfigStr(const cJSON *pRoot, const char *cfg, const char *section, const char *name) {
	char *ret = NULL;

	cJSON *pSection = cJSON_GetObjectItem(pRoot, section);
	if (pSection != NULL) {
		cJSON *pName = cJSON_GetObjectItem(pSection, name);

		if (cJSON_IsString(pName) && (pName->valuestring != NULL)) {
			ret = (char*) malloc(strlen(pName->valuestring));
			strcpy(ret, pName->valuestring);
			ESP_LOGD(TAG, "section (%s), name (%s) in (%s) found (%s)", section, name, cfg, ret);
		} else {
			ESP_LOGE(TAG, "name in (%s) not found (%s)", cfg, name);
		}
	} else {
		ESP_LOGE(TAG, "section in (%s) not found (%s)", cfg, section);
	}
	return ret;
}

int configBase::parse(const char *value) {
	if (next() != NULL) {
		return next()->parse(value);
	} else {
		return 0;
	}
}

void configBase::merge(const cJSON *patch) {
	pConfig = cJSONUtils_MergePatch(pConfig, patch);
}

char* configBase::stringify() {
	if (m_string != NULL) {
		free(m_string);
		m_string = NULL;
	}

	cJSON *mqtt = cJSON_GetObjectItem(getRoot(), name());
	m_string = cJSON_PrintUnformatted(mqtt);

	return m_string;
}

void configBase::debug() {
	char *out = cJSON_Print(pConfig);
	ESP_LOGD(TAG, "debug: (%8X) %s ", (uint32_t )pConfig, out != NULL ? out : "error");
	free(out);
}

configBase::~configBase() {
	// TODO Auto-generated destructor stub
}

esp_err_t MqttConfig::init() {

	if (!isInitialized()) {
		configBase::init();
	}
	size_t required_size = 0;
	/* read mqtt device name */
	mqtt_device_name = readString("mqtt", "device");

	if (NULL != mqtt_device_name) {
		required_size = strnlen(mqtt_device_name, MAX_DEVICE_NAME);
	} else {
		/* set default device name */
		uint8_t mac[6] = { 0 };
		esp_efuse_mac_get_default(mac);
		snprintf(def_mqtt_device, sizeof(def_mqtt_device), MQTT_PUB_MESSAGE_FORMAT, MQTT_DEVICE, mac[0], mac[1], mac[2],
				mac[3], mac[4], mac[5], "/");
		mqtt_device_name = def_mqtt_device;
		required_size = strlen(def_mqtt_device);
	}

	mqtt_sub_msg = (char*) malloc(required_size + sizeof("#") + 1);
	mqtt_pub_msg = (char*) malloc(required_size + sizeof("state/") + 1);

	if ((mqtt_sub_msg != NULL) && (mqtt_pub_msg != NULL)) {
		snprintf(mqtt_sub_msg, required_size + sizeof("#") + 1, "%s#", mqtt_device_name);
		snprintf(mqtt_pub_msg, required_size + sizeof("state/") + 1, "%sstate/", mqtt_device_name);
	}

	ESP_LOGD(TAG, "sub: (%s) pub (%s)", getSubMsg(), getPubMsg());

	return ESP_OK;
}

int MqttConfig::parse(const char* value) {
	int ret = 0;
	cJSON *patch = cJSON_Parse(value);

	if (patch != NULL && cJSON_GetObjectItem(patch, name())) {
		/* test config */
		merge(patch);
		ret = 1;
		cJSON_Delete(patch);
	} else {
		ret = configBase::parse(value);
	}
	return ret;
}

ChannelConfig::ChannelConfig() :
		configBase("channels"), m_channelCount(0) {
}

esp_err_t ChannelConfig::init() {
	esp_err_t ret = ESP_FAIL;
	if (!isInitialized()) {
		ret = configBase::init();
	}
	cJSON *pChan = cJSON_GetObjectItem(getRoot(), "channels");
	ESP_LOGD(TAG, "channels 0x%8X : isArray %d , Size %d", (unsigned int )pChan,
			pChan != NULL ? cJSON_IsArray(pChan) : false, pChan != NULL ? cJSON_GetArraySize(pChan) : 0);
	if (pChan != NULL && cJSON_IsArray(pChan)) {
		m_channelCount = cJSON_GetArraySize(pChan);
		ret = ESP_OK;
	}
	return ret;
}

int ChannelConfig::parse(const char* value) {
	int ret = 0;
	cJSON *patch = cJSON_Parse(value);

	if (patch != NULL && cJSON_GetObjectItem(patch, name())) {
		/* test config */
		merge(patch);
		ret = 1;
		cJSON_Delete(patch);
	} else {
		ret = configBase::parse(value);
	}
	return ret;
}

const char* ChannelConfig::getName(unsigned ch) {
	char *ret = cJSON_GetStringValue(const_cast<cJSON*>(getItem(ch, "name")));
	return ret != NULL ? ret : "no name";
}

const char* ChannelConfig::getAlt(unsigned ch) {
	char *ret = cJSON_GetStringValue(const_cast<cJSON*>(getItem(ch, "alt")));
	return ret != NULL ? ret : "no alt. name";
}

bool ChannelConfig::isEnabled(unsigned ch) {
	return cJSON_IsTrue(const_cast<cJSON*>(getItem(ch, "enabled")));
}

unsigned ChannelConfig::getTime(unsigned ch) {
	cJSON *pItem = const_cast<cJSON*>(getItem(ch, "maxTime"));
	return cJSON_IsNumber(pItem) ? pItem->valueint : 0;
}


const cJSON* ChannelConfig::getItem(unsigned ch, const char *item) {
	cJSON *ret = NULL;
	if (isInitialized() && ch < m_channelCount) {
		cJSON *pChan = cJSON_GetObjectItem(getRoot(), "channels");
		cJSON *pItem = cJSON_GetArrayItem(pChan, ch);
		ret = cJSON_GetObjectItem(pItem, item);
	}
	return ret;
}

SensorConfig::SensorConfig() :
		configBase("sensors"), m_sensorCount(0) {
}

esp_err_t SensorConfig::init() {
	esp_err_t ret = ESP_FAIL;
	if (!isInitialized()) {
		ret = configBase::init();
	}
	cJSON *pChan = cJSON_GetObjectItem(getRoot(), "sensors");
	ESP_LOGD(TAG, "sensors 0x%8X : isArray %d , Size %d", (unsigned int )pChan,
			pChan != NULL ? cJSON_IsArray(pChan) : false, pChan != NULL ? cJSON_GetArraySize(pChan) : 0);
	if (pChan != NULL && cJSON_IsArray(pChan)) {
		m_sensorCount = cJSON_GetArraySize(pChan);
		ret = ESP_OK;
	}
	return ret;
}

int SensorConfig::parse(const char* value) {
	int ret = 0;
	cJSON *patch = cJSON_Parse(value);

	if (patch != NULL && cJSON_GetObjectItem(patch, name())) {
		/* test config */
		merge(patch);
		ret = 1;
		cJSON_Delete(patch);
	} else {
		ret = configBase::parse(value);
	}
	return ret;
}

bool SensorConfig::isSHT1xEnabled() {
	const cJSON *item = getItem("sht1x", "enabled");
	ESP_LOGD(TAG, "sht1x: 0x%08X : %s", (unsigned int )item, cJSON_IsTrue(item) ? "true" : "false");
	return cJSON_IsTrue(item);
}

const cJSON* SensorConfig::getItem(const char *name, const char *item) {
	cJSON *ret = NULL;
	if (isInitialized()) {
		cJSON *pArray = cJSON_GetObjectItem(getRoot(), "sensors");
		cJSON *aritem = pArray ? pArray->child : 0;
		while (aritem) {
			cJSON *sensitem = cJSON_GetObjectItem(cJSON_GetObjectItem(aritem, name), item);

			if (sensitem != NULL) {
				ret = sensitem;
				break;
			}
			aritem = aritem->next;
		}
	}
	return ret;
}

int SysConfig::parse(const char *value) {
	int ret = 0;
	cJSON *patch = cJSON_Parse(value);

	if (patch != NULL && cJSON_GetObjectItem(patch, name())) {
		/* test config */
		merge(patch);
		ret = 1;
		cJSON_Delete(patch);
	} else {
		ret = configBase::parse(value);
	}
	return ret;
}


} /* namespace Config */

//Globals
Config::MqttConfig mqttConf;
Config::SysConfig sysConf;
Config::ChannelConfig chanConf;
Config::SensorConfig sensorConf;

