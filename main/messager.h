/*
 * messageHandler.h
 *
 *  Created on: 20.07.2020
 *      Author: marco
 */

#ifndef MAIN_MESSAGER_H_
#define MAIN_MESSAGER_H_

#include "config.h"
#include "mqtt_client.h"


class Messager {
public:
	Messager();
	void handle(esp_mqtt_event_handle_t event);
	int addHandler(const char *topic, Config::ParseHandler *pHandle);
	virtual ~Messager();
private:
	int handleControlMsg(const char * topic, esp_mqtt_event_handle_t event);
	void handleChannelControl(const cJSON* const chan);
};

extern Messager messager;

#endif /* MAIN_MESSAGER_H_ */
