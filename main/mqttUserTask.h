/*
 * mqtt_client.h
 *
 *  Created on: 28.06.2018
 *      Author: marco
 */

#ifndef MAIN_MQTT_USER_H_
#define MAIN_MQTT_USER_H_

#include "QueueCPP.h"
#include "ConnectionObserver.h"
#include <string>

class MainClass;

namespace mqtt {

//TODO
struct mqttMessage{
	std::string m_topic;
	std::string m_data;
	mqttMessage() = default;
	mqttMessage(const std::string &_topic, const std::string &_data) : m_topic(_topic), m_data(_data) {}
};

typedef Queue<mqttMessage,10> PubQueue;

bool isTopic(esp_mqtt_event_handle_t event, const char *pCommand);

class MqttUserTask;
class MqttConnectionObserver : public ConnectionObserver {
public:
	MqttConnectionObserver(MqttUserTask *_m) : m_mqtt(_m) {}
	virtual void onConnect();
	virtual void onDisconnect();
private:
	MqttUserTask *m_mqtt;
};

class MqttUserTask : public TaskClass {
	friend MqttConnectionObserver;
public:
	MqttUserTask() : TaskClass("mqttuser", TaskPrio_HMI, 2048), m_obs(this) {}
	virtual void task();
	void init(void);
	PubQueue& queue() { return m_pubQueue; }
	ConnectionObserver& obs() { return m_obs; }

	void send(const mqttMessage &rxData);

private:
	static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event);
	void connect(void);
	void disconnect(void);

	MqttConnectionObserver m_obs;
	PubQueue m_pubQueue;
	esp_mqtt_client_handle_t client = NULL;
	bool isMqttConnected = false;
	bool isMqttInit = false;
};

inline void MqttConnectionObserver::onConnect() {
	m_mqtt->connect();
}

inline void MqttConnectionObserver::onDisconnect() {
	m_mqtt->disconnect();
}

}
#endif /* MAIN_MQTT_USER_H_ */
