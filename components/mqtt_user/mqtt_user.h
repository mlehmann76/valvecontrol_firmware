/*
 * mqtt_client.h
 *
 *  Created on: 28.06.2018
 *      Author: marco
 */

#ifndef MAIN_MQTT_USER_H_
#define MAIN_MQTT_USER_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*pOnMessageFunc)(const char *, esp_mqtt_event_handle_t);

typedef struct {
	const char * topicName;
	pOnMessageFunc onMessage;
	const char* handlerName;
} messageHandler_t;

typedef struct {
	char *pTopic;
	char *pData;
	size_t topic_len;
	size_t data_len;
} message_t;

void mqtt_user_init(void);
void mqtt_connect(void);
void mqtt_disconnect(void);
int  mqtt_user_addHandler(messageHandler_t *pHandle);
bool isTopic(esp_mqtt_event_handle_t event, const char * pCommand);

#ifdef __cplusplus
}
#endif




#endif /* MAIN_MQTT_USER_H_ */
