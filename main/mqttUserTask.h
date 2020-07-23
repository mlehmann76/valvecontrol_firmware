/*
 * mqtt_client.h
 *
 *  Created on: 28.06.2018
 *      Author: marco
 */

#ifndef MAIN_MQTT_USER_H_
#define MAIN_MQTT_USER_H_

class MainClass;

namespace mqtt {

//TODO
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
	PubQueue& queue() { return m_pubQueue; }

private:
	static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event);

	PubQueue m_pubQueue;
	esp_mqtt_client_handle_t client = NULL;
	bool isMqttConnected = false;
	bool isMqttInit = false;
};

}
#endif /* MAIN_MQTT_USER_H_ */
