/*
 * config.cpp
 *
 *  Created on: 19.06.2020
 *      Author: marco
 */
#include <fmt/printf.h>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdlib.h>
#include <string>

#include "esp_err.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "Cipher.h"
#include "config.h"
#include "config_user.h"
#include "repository.h"
#include "utilities.h"

using namespace std::string_literals;

static const char *TAG = "CONFIG";

extern const char config_json_start[] asm("_binary_config_json_start");

namespace Config {

bool ConfigBase::m_isInitialized = false;
bool ConfigBase::m_keyReset = false;
AES128Key ConfigBase::m_key = {};
std::mutex mutex;

const char MqttConfig::MQTT_PUB_MESSAGE_FORMAT[] =
    "%s%02X%02X%02X%02X%02X%02X%s";

repository &repo() {
    static repository s_repository("", tag<ReplaceLinkPolicy>{});
    return s_repository;
}

ConfigBase::ConfigBase()
    : my_handle(), m_timeout("ConfigTimer", this, &ConfigBase::writeConfig,
                             (1000 / portTICK_PERIOD_MS), false) {
    m_isInitialized = false;
    m_keyReset = false;
}

esp_err_t ConfigBase::init() {
    std::lock_guard<std::mutex> lck(mutex);
    esp_err_t ret = ESP_OK;
    if (!m_isInitialized) {

    	spiffsInit();

        char *nvs_json_config;

        repo().create("/system/auth/config", {{{"user", "admin"s}, //
                                               {"password", "admin"s}}});
        initNVSFlash(NoForceErase);

        if (ESP_OK != readKey() ||
            ESP_OK != readStr(&my_handle, "config_json", &nvs_json_config)) {
        	//FIXME initNVSFlash(ForceErase);
            genKey();
            repo().parse(config_json_start);
            //FIXME writeConfig();
        } else {
            repo().parse(nvs_json_config);
        }

        Config::repo().addNotify("/*/*/config", Config::onConfigNotify(baseConf));

        m_isInitialized = true;
    }

    return ret;
}

void ConfigBase::initNVSFlash(forceErase_t f) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND || ForceErase == f) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        log_inst.info(TAG, "erasing nvs_flash");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        log_inst.error(TAG, "nvs_open storage failed ({})",
                       esp_err_to_name(err));
    }
}

esp_err_t ConfigBase::readKey() {
    char *pKey;
    esp_err_t err = readStr(&my_handle, "config_key", &pKey);
    if (ESP_OK == err) {
        m_key = {std::string(pKey, m_key.size())};
        log_inst.debug(TAG, "key:{}", m_key.to_hex());
    } else {
        log_inst.error(TAG, "config_key read failed ({})",
                       esp_err_to_name(err));
    }
    return err;
}

esp_err_t ConfigBase::genKey() {
    esp_err_t err = ESP_OK;
    auto key = AES128Key::genRandomKey("my esspressif key");
    if (key) {
        m_keyReset = true;
        m_key = *key;
        err = writeStr(&my_handle, "config_key", key->to_string().c_str());
        nvs_commit(my_handle);
    } else {
        log_inst.error(TAG, "config_key gen failed");
    }
    return err;
}

esp_err_t ConfigBase::readStr(nvs_handle *pHandle, const char *pName,
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

esp_err_t ConfigBase::writeStr(nvs_handle *pHandle, const char *pName,
                               const char *str) {
    return nvs_set_str(*pHandle, pName, str);
}

void ConfigBase::onConfigNotify(const std::string &s) {
    std::lock_guard<std::mutex> lock(m_lock);
    m_timeout.start();
}

void ConfigBase::writeConfig() {
    std::lock_guard<std::mutex> lock(m_lock);
    log_inst.info(TAG, "writeConfig called");
    esp_err_t err = ESP_OK;
    // FIXME err = writeStr(&my_handle, "config_json",
    // repo().stringify(repo().partial("/*/*/config")).c_str());
    if (ESP_OK != err) {
        log_inst.error(TAG, "writeConfig failed ({})", esp_err_to_name(err));
    }
    nvs_commit(my_handle);
}

void ConfigBase::spiffsInit(void) {
    log_inst.info(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {.base_path = "/spiffs",
                                  .partition_label = NULL,
                                  .max_files = 5,
                                  .format_if_mount_failed = false};

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            log_inst.error(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            log_inst.error(TAG, "Failed to find SPIFFS partition");
        } else {
            log_inst.error(TAG, "Failed to initialize SPIFFS {}",
                           esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        log_inst.error(TAG, "Failed to get SPIFFS partition information {}",
                       esp_err_to_name(ret));
    } else {
        log_inst.info(TAG, "Partition size: total: {:d}, used: {:d}", total,
                      used);
    }
}


esp_err_t MqttConfig::init() {

    static char *MQTT_DEVICE = (char *)"esp32/";

    if (!m_base.isInitialized()) {
        m_base.init();
    }
    /* read mqtt device name */

    mqtt_device_name =
        Config::repo().get<std::string>("/network/mqtt/config", "device");

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

esp_err_t NetConfig::init() {
    if (!m_base.isInitialized()) {
        m_base.init();
    }
    repo().create("/network/sntp/config", {{               //
                                            {"zone", ""s}, //
                                            {"server", ""s}}});
    repo().create("/network/mqtt/config", {{                    //
                                            {"enabled", false}, //
                                            {"server", ""s},    //
                                            {"user", ""s},      //
                                            {"pass", ""s},      //
                                            {"device", ""s}}});
    repo().create("/network/wifi/config", {{                            //
                                            {"hostname", "espressif"s}, //
                                            {"mode", 2}}});
    repo().create("/network/wifi/config/AP", {{                          //
                                               {"ssid", "espressifAP"s}, //
                                               {"pass", "espressif"s},   //
                                               {"channel", 1}}});
    repo().create("/network/wifi/config/STA", {{               //
                                                {"ssid", ""s}, //
                                                {"pass", ""s}}});

    // saving password will encrypt it
    repo()["/network/wifi/config/AP"].set(doEncrypt(*this, "pass"));

    // saving password will encrypt it
    repo()["/network/wifi/config/STA"].set(doEncrypt(*this, "pass"));

    if (m_base.isKeyReset()) {
        Cipher ciph(m_base.key());
        repo()["/network/wifi/config/AP"]["pass"] = "espressif"s;
    }

    return ESP_OK;
}

std::optional<property> NetConfig::doEncrypt::operator()(const property &p) {
    auto it = p.find(key);
    if (it != p.end() && it->second.is<StringType>()) {
        Cipher ciph(net.key());
        auto temp = p;
        temp[key] = toHex(ciph.encrypt(it->second.get_unchecked<StringType>()));
        return temp;
    }
    return {};
}

std::string NetConfig::getTimeServer() const {
    return repo().get<std::string>("/network/sntp/config", "server");
}

std::string NetConfig::getTimeZone() const {
    return repo().get<std::string>("/network/sntp/config", "zone");
}

std::string NetConfig::getHostname() const {
    return repo().get<std::string>("/network/wifi/config", "hostname");
}

std::string NetConfig::getApSSID() const {
    return repo().get<std::string>("/network/wifi/config/AP", "ssid",
                                   "espressifAP");
}

std::string NetConfig::getApPass() const {
    Cipher cipher = {key()};
    return cipher.decrypt(
        fromHex(repo().get<std::string>("/network/wifi/config/AP", "pass")));
}

unsigned NetConfig::getApChannel() const {
    return repo().get<IntType>("/network/wifi/config/AP", "channel", 1);
}

std::string NetConfig::getStaSSID() const {
    return repo().get<std::string>("/network/wifi/config/STA", "ssid");
}

std::string NetConfig::getStaPass() const {
    Cipher cipher = {key()};
    return cipher.decrypt(
        fromHex(repo().get<std::string>("/network/wifi/config/STA", "pass")));
}

unsigned NetConfig::getMode() const {
    return repo().get<IntType>("/network/wifi/config", "mode", 1);
}

esp_err_t ChannelConfig::init() {
    esp_err_t ret = ESP_FAIL;
    if (!m_base.isInitialized()) {
        ret = m_base.init();
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
    _name << "/actors/channel" << ch + 1 << "/config";
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

std::string SysConfig::getPass() {
    return repo().get<std::string>("/system/auth/config", "password");
}

std::string SysConfig::getUser() {
    return repo().get<std::string>("/system/auth/config", "user");
}

} /* namespace Config */

// Globals
Config::ConfigBase baseConf;
Config::MqttConfig mqttConf(baseConf);
Config::SysConfig sysConf(baseConf);
Config::ChannelConfig chanConf(baseConf);
Config::NetConfig netConf(baseConf);
