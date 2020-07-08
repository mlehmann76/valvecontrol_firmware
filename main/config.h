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

class ParseHandler {
public:
	ParseHandler(const char *_name) : m_name(_name), m_next() {}
	virtual ~ParseHandler() {	}
	virtual int parse(const char*) = 0;
	const char* name() const {
		return m_name;
	}
	ParseHandler* setNext(ParseHandler *_handler) {
		m_next = _handler;
		return m_next;
	}
	ParseHandler* next() const {
		return m_next;
	}
private:
	const char *m_name;
	ParseHandler *m_next;
};


class configBase: public ParseHandler {
	static bool m_isInitialized;
	static cJSON *pConfig;
public:
	configBase(const char *_name);
	virtual ~configBase();
	virtual esp_err_t init();
	virtual char* stringify();
	virtual int parse(const char*);
	char* readString(const char *section, const char *name);
	const cJSON* getRoot() const {
		return pConfig;
	}
	bool isInitialized() const {
		return m_isInitialized;
	}

protected:
	void merge(const cJSON*);
	void debug();
	char *m_string;
private:
	esp_err_t readStr(nvs_handle *pHandle, const char *pName, char **dest);
	esp_err_t writeStr(nvs_handle *pHandle, const char *pName, const char *str);
	esp_err_t readConfig(const char *section, const char *name, char **dest);
	char* readJsonConfigStr(const cJSON *pRoot, const char *cfg, const char *section, const char *name);
	nvs_handle_t my_handle;
};

class SysConfig: public configBase {
public:
	SysConfig() :
			configBase("system") {
	}

	virtual int parse(const char*);

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

class MqttConfig: public configBase {
	static const char MQTT_PUB_MESSAGE_FORMAT[];
	static const size_t MAX_DEVICE_NAME = 32;

public:
	MqttConfig() :
			configBase("mqtt") {
	}
	virtual esp_err_t init();

	virtual int parse(const char*);

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

class ChannelConfig: public configBase {
public:
	ChannelConfig();
	virtual esp_err_t init();
	virtual int parse(const char*);
	const char* getName(unsigned ch);
	const char* getAlt(unsigned ch);
	bool isEnabled(unsigned ch);
	unsigned getTime(unsigned ch);
	unsigned count() const {
		return m_channelCount;
	}
private:
	const cJSON* getItem(unsigned ch, const char *item);
	unsigned m_channelCount;
};

class SensorConfig: public configBase {
public:
	SensorConfig();
	virtual esp_err_t init();
	virtual int parse(const char*);
	bool isSHT1xEnabled();
private:
	const cJSON* getItem(const char *name, const char *item);
	unsigned m_sensorCount;
};

} /* namespace Config */

extern Config::MqttConfig mqttConf;
extern Config::SysConfig sysConf;
extern Config::ChannelConfig chanConf;
extern Config::SensorConfig sensorConf;

#endif /* MAIN_CONFIG_H_ */
