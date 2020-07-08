/*
 * mqtt_client.h
 *
 *  Created on: 28.06.2018
 *      Author: marco
 */

#ifndef MAIN_MQTT_USER_H_
#define MAIN_MQTT_USER_H_

struct messageHandler{
	const char * topicName;
	Config::ParseHandler *handler;
	messageHandler(const char *_topicName = "none", Config::ParseHandler *_handler = NULL) : topicName(_topicName), handler(_handler) {}
};

typedef struct {
	char *pTopic;
	char *pData;
	size_t topic_len;
	size_t data_len;
} message_t;

void mqtt_user_init(void);
void mqtt_connect(void);
void mqtt_disconnect(void);
int  mqtt_user_addHandler(const char *topic, Config::ParseHandler *pHandle);
bool isTopic(esp_mqtt_event_handle_t event, const char * pCommand);

#endif /* MAIN_MQTT_USER_H_ */
