/*
 * mqtt_client.h
 *
 *  Created on: 28.06.2018
 *      Author: marco
 */

#ifndef MAIN_MQTT_USER_H_
#define MAIN_MQTT_USER_H_

namespace mqtt {

struct messageHandler {
	const char *topicName;
	Config::ParseHandler *handler;
	messageHandler(const char *_topicName = "none", Config::ParseHandler *_handler = NULL) :
			topicName(_topicName), handler(_handler) {
	}
};

typedef struct {
	char *pTopic;
	char *pData;
	size_t topic_len;
	size_t data_len;
} message_t;

typedef Queue<message_t,10> PubQueue;

bool isTopic(esp_mqtt_event_handle_t event, const char *pCommand);

class MqttUserTask : public TaskClass {
public:
	MqttUserTask() : TaskClass("mqttuser", TaskPrio_HMI, 2048) {}
	virtual void task();
	void init(void);
	void connect(void);
	void disconnect(void);
	int addHandler(const char *topic, Config::ParseHandler *pHandle);
	PubQueue& queue() { return m_pubQueue; }

private:
	void handler(esp_mqtt_event_handle_t event);
	static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event);

	PubQueue m_pubQueue;
	esp_mqtt_client_handle_t client = NULL;
	messageHandler messageHandle[8];
	bool isMqttConnected = false;
	bool isMqttInit = false;
};

}
#endif /* MAIN_MQTT_USER_H_ */
