/*
 * config.h
 *
 *  Created on: 19.06.2020
 *      Author: marco
 */

#ifndef MAIN_CONFIG_H_
#define MAIN_CONFIG_H_

#include "cJSON.h"
#include "nvs.h"

namespace Config {

class configBase {
private:
	static bool m_isInitialized;
	static cJSON *pConfig;
	esp_err_t readStr(nvs_handle *pHandle, const char *pName, char **dest);
	esp_err_t writeStr(nvs_handle *pHandle, const char *pName, const char *str);
	char* readJsonConfigStr(const cJSON *pRoot, const char *cfg, const char *section, const char *name) ;
public:
	configBase();
	virtual esp_err_t init();
	virtual ~configBase();
	virtual char* stringify() = 0;
	virtual void parse(const char*);
	esp_err_t readConfig(const char *section, const char *name, char **dest) ;
	char* readString(const char *section, const char *name);
	const cJSON* getRoot() const {
		return pConfig;
	}
	bool isInitialized() const {
		return m_isInitialized;
	}
	void merge(const cJSON*);
	void debug();
protected:
	nvs_handle_t my_handle;
	char *m_string;
};

class SysConfig: protected configBase {
public:
	virtual esp_err_t init() {
		return ESP_OK;
	}
	virtual char* stringify();
	const char* getUser() {
		return readString("system", "user");
	}
	const char* getPass() {
		return readString("system", "pass");
	}
	const char* getTimeZone() {
		return readString("sntp", "zone");
	}
	const char* getTimeServer() {
		return readString("sntp", "server");
	}
private:
};

class MqttConfig: protected configBase {
	static const char MQTT_PUB_MESSAGE_FORMAT[];

public:
	virtual esp_err_t init();
	virtual char* stringify();

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
		return readString("mqtt", "server");
	}

	const char* getMqttUser() {
		return readString("mqtt", "user");
	}

	const char* getMqttPass() {
		return readString("mqtt", "pass");
	}

private:
	char def_mqtt_device[64] = { 0 }; //TODO
	char *mqtt_sub_msg = NULL;
	char *mqtt_pub_msg = NULL;
	char *mqtt_device_name = NULL;
	char *MQTT_DEVICE = (char*) "esp32/";
};

class ChannelConfig: protected configBase {
public:
	ChannelConfig();
	virtual esp_err_t init();
	virtual char* stringify();
	const char* getName(unsigned ch);
	const char* getAlt(unsigned ch);
	bool isEnabled(unsigned ch);
	unsigned getTime(unsigned ch);
	unsigned count() const { return m_channelCount; }
private:
	const cJSON* getItem(unsigned ch, const char* item);
	unsigned m_channelCount;
};

class SensorConfig: protected configBase {
public:
	SensorConfig();
	virtual esp_err_t init();
	virtual char* stringify();
	bool isSHT1xEnabled();
private:
	const cJSON* getItem(const char *name, const char* item);
	unsigned m_sensorCount;
};

} /* namespace Config */

extern Config::MqttConfig mqtt;
extern Config::SysConfig sys;
extern Config::ChannelConfig chanConfig;
extern Config::SensorConfig sensorConfig;

#endif /* MAIN_CONFIG_H_ */
