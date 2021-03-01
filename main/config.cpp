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
#include <iostream>
#include <fstream>

#include "esp_err.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_image_format.h"
#include "sdkconfig.h"

#include "Cipher.h"
#include "config.h"
#include "config_user.h"
#include "repository.h"
#include "utilities.h"
#include "version.h"

using namespace std::string_literals;

static const char *TAG = "CONFIG";

extern const char config_json_start[] asm("_binary_config_json_start");

namespace Config {

bool ConfigBase::m_isInitialized = false;
bool ConfigBase::m_keyReset = false;
Cipher ConfigBase::m_crypt = {AES128Key{}};
std::mutex mutex;

const char MqttConfig::MQTT_PUB_MESSAGE_FORMAT[] =
    "%s%02X%02X%02X%02X%02X%02X%s";

repository &repo() {
    static repository s_repository("", tag<ReplaceLinkPolicy>{});
    return s_repository;
}


std::string bin2String(std::string s) {
	return std::move(utilities::base64_encode(std::move(s)));
}

std::string string2Bin(std::string s) {
	return std::move(utilities::base64_decode(std::move(s)));
}


ConfigBase::ConfigBase()
    : my_handle(), m_timeout("ConfigTimer", this, &ConfigBase::onTimeout,
                             (1000 / portTICK_PERIOD_MS), false) {
    m_isInitialized = false;
    m_keyReset = false;
}

esp_err_t ConfigBase::init() {
    std::lock_guard<std::mutex> lck(mutex);
    esp_err_t ret = ESP_OK;
	esp_chip_info_t chip_info;
	esp_chip_info(&chip_info);

	if (!m_isInitialized) {

    	spiffsInit();

        repo().create("/system/auth/config",
                      {{{"user", "admin"s}, {"password", "admin"s}}});

        repo().create("/system/version/state",
                      {{{"date", std::string(__DATE__)},
                        {"time", std::string(__TIME__)},
                        {"version", std::string(VALVECONTROL_GIT_TAG)},
                        {"idf", std::string(esp_get_idf_version())},
                        {"cores", static_cast<IntType>(chip_info.cores)},
                        {"rev", static_cast<IntType>(chip_info.revision)}}});

        repo().create("/system/base/control/resetToDefaults", {{{"start", false}}})
                    .set([this](const property &) -> std::optional<property> {
                		this->resetToDefault();
                        return {};
                    });

        initNVSFlash(NoForceErase);
        std::string str;

        if (ESP_OK != readKey()) {
            genKey();
        }

        if (ESP_OK != readConfig(str) || isKeyReset()) {
            repo().parse(config_json_start);
            writeConfig();
        } else {
            repo().parse(str);
        }

        Config::repo().addNotify("/*/*/config",
        		Config::onConfigNotify(baseConf));

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
        m_crypt = {AES128Key{std::string(pKey, AES128Key::size())}};
        log_inst.debug(TAG, "key:{}", m_crypt.key().to_hex());
    } else {
        log_inst.error(TAG, "config_key read failed ({})",
                       esp_err_to_name(err));
    }
    return err;
}

esp_err_t ConfigBase::genKey() {
    esp_err_t err = ESP_OK;
    auto key = AES128Key::genRandomKey("my esspressif key"); //FIXME
    if (key) {
        m_keyReset = true;
        m_crypt = {*key};
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

void ConfigBase::resetToDefault() {
	//deleting the key will force reset to defaults
	esp_err_t err =
			nvs_erase_key(my_handle, "config_key");
    nvs_commit(my_handle);
    esp_restart();
}

void ConfigBase::onTimeout() {
	writeConfig();
}

std::string ConfigBase::configFileName() const {
	return std::move("/spiffs/"+bin2String(m_crypt.encrypt("config.json")));
}

esp_err_t ConfigBase::writeConfig() {
    std::lock_guard<std::mutex> lock(m_lock);
	const std::string fname = configFileName();
    esp_err_t err = ESP_OK;
    log_inst.info(TAG, "writeConfig called {}", fname);
    std::ofstream ofs(fname, std::ios::out |std::ios::binary | std::ios::trunc);
    if (ofs.is_open()) {
    	ofs << repo().stringify({"/*/*/config"},0);
        ofs.close();
    } else {
        log_inst.error(TAG, "writeConfig failed");
        err = ESP_FAIL;
    }
    return err;
}

esp_err_t ConfigBase::readConfig(std::string& str) {
	const std::string fname = configFileName();
    std::ifstream ifs(fname, std::ios::in|std::ios::binary);
    log_inst.info(TAG, "readConfig called {}", fname);
    if (ifs.is_open()) {
        // get length of file:
        ifs.seekg(0, ifs.end);
        size_t len = ifs.tellg();
        str.reserve(len);
        log_inst.info(TAG, "readConfig length {:d}", len);
        ifs.seekg(0, ifs.beg);

        str.assign((std::istreambuf_iterator<char>(ifs)),
                    std::istreambuf_iterator<char>());

        ifs.close();
        return len > 0 ? ESP_OK : ESP_FAIL;
    }
    log_inst.info(TAG, "readConfig failed {}", fname);
    return ESP_FAIL;
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

std::string MqttConfig::getPass() const {
    return crypt().decrypt(
        bin2String(repo().get<std::string>("/network/mqtt/config", "pass")));
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
	repo()["/network/mqtt/config"].set(doEncrypt(*this, "pass"));

    // saving password will encrypt it
    repo()["/network/wifi/config/AP"].set(doEncrypt(*this, "pass"));

    // saving password will encrypt it
	repo()["/network/wifi/config/STA"].set(doEncrypt(*this, "pass"));

    if (m_base.isKeyReset()) {
        setApPass("espressif"); //FIXME make configurable
    }

    return ESP_OK;
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
    return crypt().decrypt(
        string2Bin(repo().get<std::string>("/network/wifi/config/AP", "pass")));
}

void NetConfig::setApPass(std::string s) {
	repo()["/network/wifi/config/AP"]["pass"] = std::move(s);
}


unsigned NetConfig::getApChannel() const {
    return repo().get<IntType>("/network/wifi/config/AP", "channel", 1);
}

std::string NetConfig::getStaSSID() const {
    return repo().get<std::string>("/network/wifi/config/STA", "ssid");
}

std::string NetConfig::getStaPass() const {
    return crypt().decrypt(
    	string2Bin(repo().get<std::string>("/network/wifi/config/STA", "pass")));
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
                                              {"enabled", false},
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
    return crypt().decrypt(
    	string2Bin(repo().get<std::string>("/system/auth/config", "password")));
}

std::string SysConfig::getUser() {
    return repo().get<std::string>("/system/auth/config", "user");
}

esp_err_t SysConfig::init() {
    if (!m_base.isInitialized()) {
        m_base.init();
    }
    // saving password will encrypt it
	repo()["/system/auth/config"].set(doEncrypt(*this, "password"));

    if (m_base.isKeyReset()) {
    	repo()["/system/auth/config"]["user"] = "admin"s;
    	repo()["/system/auth/config"]["password"] = "admin"s;
    }

	return ESP_OK;
}

} /* namespace Config */

// Globals
Config::ConfigBase baseConf;
Config::MqttConfig mqttConf(baseConf);
Config::SysConfig sysConf(baseConf);
Config::ChannelConfig chanConf(baseConf);
Config::NetConfig netConf(baseConf);
