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

class AbstractMqttReceiver {
public:
	virtual ~AbstractMqttReceiver() = default;
	virtual int onMessage(esp_mqtt_event_handle_t event) = 0;
};

class Messager {
public:
	Messager();
	void handle(esp_mqtt_event_handle_t event);
	void addHandle(AbstractMqttReceiver *);
	virtual ~Messager();
private:
	std::vector<AbstractMqttReceiver*> m_mqttRec;
	AbstractMqttReceiver* m_lastMqttRec = nullptr;
};

#endif /* MAIN_MESSAGER_H_ */
