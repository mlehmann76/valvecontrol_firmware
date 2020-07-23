/*
 * messageHandler.h
 *
 *  Created on: 20.07.2020
 *      Author: marco
 */

#ifndef MAIN_MESSAGER_H_
#define MAIN_MESSAGER_H_

#include <vector>
#include "config.h"
#include "mqtt_client.h"

class MqttChannelAdapter;


class Messager {
public:
	Messager();
	void handle(esp_mqtt_event_handle_t event);
	void addHandle(MqttChannelAdapter *);
	virtual ~Messager();
private:
	int handleControlMsg(const char * topic, esp_mqtt_event_handle_t event);
	void handleChannelControl(const cJSON* const chan);

	std::vector<MqttChannelAdapter*> m_mqttAdapter;
};

#endif /* MAIN_MESSAGER_H_ */
