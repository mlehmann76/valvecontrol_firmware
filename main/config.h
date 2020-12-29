/*
 * config.h
 *
 *  Created on: 19.06.2020
 *      Author: marco
 */

#ifndef MAIN_CONFIG_H_
#define MAIN_CONFIG_H_

#include "nvs.h"
#include "repository.h"
#include <chrono>
#include <memory>
#include <string>

class repository;

namespace Config {

repository &repo();

class configBase {
    static bool m_isInitialized;

  public:
    configBase(const char *_name);
    virtual ~configBase(){};
    esp_err_t init();

  protected:
    bool isInitialized() const { return m_isInitialized; }

  private:
    esp_err_t readStr(nvs_handle *pHandle, const char *pName, char **dest);
    esp_err_t writeStr(nvs_handle *pHandle, const char *pName, const char *str);
    nvs_handle_t my_handle;
};

class SysConfig : public configBase {
  public:
    SysConfig() : configBase("system") {}

    std::string getUser();
    std::string getPass();

  private:
};

class MqttConfig : public configBase {
    static const char MQTT_PUB_MESSAGE_FORMAT[];
    static const size_t MAX_DEVICE_NAME = 32;

  public:
    MqttConfig() : configBase("mqtt") {}
    esp_err_t init();

    std::string getPubMsg() const { return mqtt_pub_msg; }
    std::string getDevName() const { return mqtt_device_name; }
    void setConnected(bool isCon) {
        repo()["/network/mqtt/state"]["connected"] = isCon;
    }

  private:
    std::string mqtt_pub_msg;
    std::string mqtt_device_name;
};

class NetConfig : public configBase {
  public:
    NetConfig() : configBase("network") {}
    esp_err_t init();
    std::string getTimeZone() const;
    std::string getTimeServer() const;
    std::string getHostname() const;
    std::string getApSSID() const;
    std::string getApPass() const;
    unsigned    getApChannel() const;
    std::string getStaSSID() const;
    std::string getStaPass() const;
    unsigned getMode() const;
};

class ChannelConfig : public configBase {
  public:
    ChannelConfig();
    virtual esp_err_t init();
    std::string getName(unsigned ch);
    std::string getAlt(unsigned ch);
    bool isEnabled(unsigned ch);
    std::chrono::seconds getTime(unsigned ch);

  private:
    unsigned m_channelCount;

    std::stringstream channelName(unsigned ch);
};

} // namespace Config

extern Config::MqttConfig mqttConf;
extern Config::SysConfig sysConf;
extern Config::NetConfig netConf;
extern Config::ChannelConfig chanConf;

#endif /* MAIN_CONFIG_H_ */
