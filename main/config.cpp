/*
 * config.cpp
 *
 *  Created on: 19.06.2020
 *      Author: marco
 */
#include <fmt/printf.h>
#include <memory>
#include <sstream>
#include <stdlib.h>
#include <string>

#include "esp_err.h"
#include "esp_system.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "config.h"
#include "config_user.h"
#include "repository.h"
#include "utilities.h"

using namespace std::string_literals;

static const char *TAG = "CONFIG";

extern const char config_json_start[] asm("_binary_config_json_start");

namespace Config {

bool configBase::m_isInitialized = false;
const char MqttConfig::MQTT_PUB_MESSAGE_FORMAT[] =
    "%s%02X%02X%02X%02X%02X%02X%s";

configBase::configBase(const char *_name) : my_handle() {
    m_isInitialized = false;
}

esp_err_t configBase::init() {

    char *nvs_json_config;
    esp_err_t ret = ESP_OK;

    repo().create("system", {{{"user", "admin"s}, {"password", "admin"s}}});
    repo().create("sntp", {{{"zone", ""s}, {"server", ""s}}});
    repo().create(
        "mqtt",
        {{{"server", ""s}, {"user", ""s}, {"pass", ""s}, {"device", ""s}}});

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    ESP_ERROR_CHECK(err);

    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        log_inst.error(TAG, "nvs_open storage failed ({})", esp_err_to_name(err));
    }
#if 0
	//try opening saved json config
	err = readStr(&my_handle, "config_json", &nvs_json_config);
	if (ESP_OK != err) {
		log_inst.error(TAG, "config_json read failed ({})", esp_err_to_name(err));
//		pConfig = cJSON_Parse(config_json_start);
		repo().parse(config_json_start);
	} else {
//		pConfig = cJSON_Parse(config_json_start);
//		cJSON *patch = NULL;
//		patch = cJSON_Parse(nvs_json_config);
//		pConfig = cJSONUtils_MergePatch(pConfig, patch);
//		cJSON_Delete(patch);
		repo().parse(nvs_json_config);
	}
#else
    repo().parse(config_json_start);
#endif
    m_isInitialized = true;
    log_inst.debug(TAG, "repo ({})", repo().debug());

    return ret;
}

esp_err_t configBase::readStr(nvs_handle *pHandle, const char *pName,
                              char **dest) {
    size_t required_size;
    char *temp;
    esp_err_t err = nvs_get_str(*pHandle, pName, NULL, &required_size);
    if (err == ESP_OK) {
        temp = (char *)malloc(required_size);
        if (temp != NULL) {
            nvs_get_str(*pHandle, pName, temp, &required_size);
            *dest = temp;
        } else {
            err = ESP_ERR_NO_MEM;
        }
    }
    return err;
}

repository &repo() {
    static repository s_repository("/config", tag<ReplaceLinkPolicy>{});
    return s_repository;
}

esp_err_t configBase::writeStr(nvs_handle *pHandle, const char *pName,
                               const char *str) {
    return nvs_set_str(*pHandle, pName, str);
}

esp_err_t MqttConfig::init() {

    static char *MQTT_DEVICE = (char *)"esp32/";

    if (!isInitialized()) {
        configBase::init();
    }
    /* read mqtt device name */

    mqtt_device_name = Config::repo().get<std::string>("mqtt", "device");

    if (mqtt_device_name == "") {
        /* set default device name */
        uint8_t mac[6] = {0};
        esp_efuse_mac_get_default(mac);
        mqtt_device_name = fmt::sprintf(MQTT_PUB_MESSAGE_FORMAT, //
                                        MQTT_DEVICE, mac[0], mac[1], mac[2],
                                        mac[3], mac[4], mac[5], "/");
    }

    mqtt_pub_msg = fmt::format("{}state", mqtt_device_name);

    return ESP_OK;
}

ChannelConfig::ChannelConfig() : configBase("channels"), m_channelCount(0) {}

esp_err_t ChannelConfig::init() {
    esp_err_t ret = ESP_FAIL;
    if (!isInitialized()) {
        ret = configBase::init();
    }
    for (unsigned i = 0; i < 4; i++) {
        repo().create(channelName(i).str(), {{{"name", "no name"s},
                                              {"alt", "alt name"s},
                                              {"enabled", BoolType(false)},
                                              {"maxTime", 0}}});
    }
    return ret;
}

std::stringstream ChannelConfig::channelName(unsigned ch) {
    std::stringstream _name;
    _name << "channel" << ch + 1;
    return _name;
}

std::string ChannelConfig::getName(unsigned ch) {
    return repo().get<std::string>(channelName(ch).str(), "name");
}

std::string ChannelConfig::getAlt(unsigned ch) {
    return repo().get<std::string>(channelName(ch).str(), "alt");
}

bool ChannelConfig::isEnabled(unsigned ch) {
    return repo().get<BoolType>(channelName(ch).str(), "enabled");
}

std::chrono::seconds ChannelConfig::getTime(unsigned ch) {
    return std::chrono::seconds(
        repo().get<int>(channelName(ch).str(), "maxTime"));
}

std::string SysConfig::getTimeServer() {
    return repo().get<std::string>("sntp", "server");
}

std::string SysConfig::getTimeZone() {
    return repo().get<std::string>("sntp", "zone");
}

std::string SysConfig::getPass() {
    return repo().get<std::string>("system", "password");
}

std::string SysConfig::getUser() {
    return repo().get<std::string>("system", "user");
}

} /* namespace Config */

// Globals
Config::MqttConfig mqttConf;
Config::SysConfig sysConf;
Config::ChannelConfig chanConf;
