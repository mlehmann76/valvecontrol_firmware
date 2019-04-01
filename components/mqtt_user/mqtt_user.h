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

EventGroupHandle_t wifi_event_group;
EventGroupHandle_t mqtt_event_group;

#define MQTT_CONNECTED_BIT (1u<<0)

typedef int* pCtx_t;
typedef int (*pOnMessageFunc)(pCtx_t, esp_mqtt_event_handle_t);

typedef struct {
	pCtx_t pUserctx;
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
int  mqtt_user_addHandler(messageHandler_t *pHandle);

#ifdef __cplusplus
}
#endif




#endif /* MAIN_MQTT_USER_H_ */
