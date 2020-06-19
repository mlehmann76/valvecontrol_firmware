/*
 * config.h
 *
 *  Created on: 19.06.2020
 *      Author: marco
 */

#ifndef MAIN_CONFIG_H_
#define MAIN_CONFIG_H_

namespace Config {

class configBase {
public:
	configBase();
	virtual ~configBase();

};

class SysConfig: private configBase {

};

class MqttConfig: private configBase {
	static const char MQTT_PUB_MESSAGE_FORMAT[];

public:
	void init();
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

#endif /* MAIN_CONFIG_H_ */
