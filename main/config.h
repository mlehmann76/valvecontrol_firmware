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
	nvs_handle_t my_handle;
	cJSON *pConfig;
protected:
	static bool m_isInitialized;
public:
	configBase() ;
	virtual esp_err_t init();
	virtual ~configBase();
	virtual char* stringify() = 0;
	virtual void parse(const char*) = 0;
	esp_err_t readConfigStr(const char* section, const char* name, char **dest);
protected:
	esp_err_t readStr(nvs_handle *pHandle, const char *pName, char **dest);
	esp_err_t writeStr(nvs_handle *pHandle, const char *pName, const char *str);
	char* readJsonConfigStr(const cJSON *pRoot, const char *cfg, const char* section, const char* name);
};


class SysConfig: public configBase {
public:
	virtual esp_err_t init();
	virtual char* stringify();
	virtual void parse(const char*);
};

class MqttConfig: public configBase {
	static const char MQTT_PUB_MESSAGE_FORMAT[];

public:
	virtual esp_err_t init();
	virtual char* stringify();
	virtual void parse(const char*);

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
		return mqtt_server;
	}

	const char* getMqttUser() {
		return mqtt_user;
	}

	const char* getMqttPass() {
		return mqtt_pass;
	}
private:
	char def_mqtt_device[64] = { 0 }; //TODO
	char *mqtt_server = (char*)"mqtt://raspberrypi.fritz.box";
	char *mqtt_sub_msg = NULL;
	char *mqtt_pub_msg = NULL;
	char *mqtt_device_name = NULL;
	char *mqtt_user = (char*)"sensor1";
	char *mqtt_pass = (char*)"sensor1";
	char *MQTT_DEVICE = (char*)"esp32/";
};


} /* namespace Config */

extern Config::MqttConfig mqtt;
extern Config::SysConfig sys;

#endif /* MAIN_CONFIG_H_ */
