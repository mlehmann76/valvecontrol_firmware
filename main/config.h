/*
 * config.h
 *
 *  Created on: 19.06.2020
 *      Author: marco
 */

#ifndef MAIN_CONFIG_H_
#define MAIN_CONFIG_H_

#include "TimerCPP.h"
#include "Cipher.h"
#include "nvs.h"
#include "repository.h"
#include <chrono>
#include <memory>
#include <string>
#include "utilities.h"

class repository;

namespace Config {

repository &repo();

std::string bin2String(std::string s);
std::string string2Bin(std::string s);

template <typename T>
struct doEncrypt {
    doEncrypt(const T &_net, std::string _key)
        : net(_net), key(std::move(_key)) {}
    std::optional<property> operator()(const property &p) {
        auto it = p.find(key);
        if (it != p.end() && it->second.is<StringType>()) {
            property temp;
            temp[key] = std::move(bin2String(
                net.crypt().encrypt(it->second.get_unchecked<StringType>())));
            return temp;
        }
        return {};
    }
    const T &net;
    const std::string key;
};

class ConfigBase {
    static bool m_isInitialized;
    static Cipher m_crypt;
    static bool m_keyReset;
    enum forceErase_t { NoForceErase, ForceErase };

  public:
    ConfigBase();
    virtual ~ConfigBase(){};
    esp_err_t init();

    bool isInitialized() const { return m_isInitialized; }
    bool isKeyReset() const { return m_keyReset; }
    const Cipher& crypt() const { return m_crypt; }
    void onConfigNotify(const std::string &s);

  private:
    esp_err_t readStr(nvs_handle *pHandle, const char *pName, char **dest);
    esp_err_t writeStr(nvs_handle *pHandle, const char *pName, const char *str);
    esp_err_t readKey();
    esp_err_t genKey();
    void initNVSFlash(forceErase_t);
    void onTimeout();
    esp_err_t writeConfig();
    esp_err_t readConfig(std::string&);
    void spiffsInit();
    std::string configFileName() const;

    nvs_handle_t my_handle;
    TimerMember<ConfigBase> m_timeout;
    std::mutex m_lock;

};

struct onConfigNotify {
    onConfigNotify(ConfigBase &b) : m_base(b) {}
    void operator()(const std::string &s) { m_base.onConfigNotify(s); }
    ConfigBase &m_base;
};

class SysConfig {
  public:
    SysConfig(ConfigBase &base) : m_base(base) {}

    std::string getUser();
    std::string getPass();
    const Cipher& crypt() const { return m_base.crypt(); }

  private:
    ConfigBase &m_base;
};

class MqttConfig {
    static const char MQTT_PUB_MESSAGE_FORMAT[];
    static const size_t MAX_DEVICE_NAME = 32;

  public:
    MqttConfig(ConfigBase &base) : m_base(base) {}
    esp_err_t init();

    std::string getPubMsg() const { return mqtt_pub_msg; }
    std::string getDevName() const { return mqtt_device_name; }
    std::string getPass() const;
    std::string getUser() const {
    	return repo().get<std::string>("/network/mqtt/config", "user");
    }
    std::string getServer() const {
    	return repo().get<std::string>("/network/mqtt/config", "server");
    }

    void setConnected(bool isCon) {
        repo()["/network/mqtt/state"]["connected"] = isCon;
    }
    bool enabled() const {
        return repo()["/network/mqtt/config"]["enabled"].get<BoolType>();
    }
    const Cipher& crypt() const { return m_base.crypt(); }

  private:
    ConfigBase &m_base;
    std::string mqtt_pub_msg;
    std::string mqtt_device_name;
};

class NetConfig {
  public:
    NetConfig(ConfigBase &base) : m_base(base) {}
    esp_err_t init();
    std::string getTimeZone() const;
    std::string getTimeServer() const;
    std::string getHostname() const;
    std::string getApSSID() const;
    std::string getApPass() const;
    void setApPass(std::string s);
    unsigned getApChannel() const;
    std::string getStaSSID() const;
    void setStaSSID(const std::string& s) {
    	repo()["/network/wifi/config/STA"]["ssid"] = s;
    }
    std::string getStaPass() const;
    void setStaPass(const std::string& s) {
    	repo()["/network/wifi/config/STA"]["pass"] = s;
    }
    unsigned getMode() const;
    const Cipher& crypt() const { return m_base.crypt(); }

  private:
    ConfigBase &m_base;
};

class ChannelConfig {
  public:
    ChannelConfig(ConfigBase &base) : m_base(base), m_channelCount() {}
    esp_err_t init();
    std::string getName(unsigned ch);
    std::string getAlt(unsigned ch);
    bool isEnabled(unsigned ch);
    std::chrono::seconds getTime(unsigned ch);

  private:
    ConfigBase &m_base;
    unsigned m_channelCount;

    std::stringstream channelName(unsigned ch);
};

} // namespace Config

extern Config::ConfigBase baseConf;
extern Config::MqttConfig mqttConf;
extern Config::SysConfig sysConf;
extern Config::NetConfig netConf;
extern Config::ChannelConfig chanConf;

#endif /* MAIN_CONFIG_H_ */
