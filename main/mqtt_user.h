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

extern QueueHandle_t subQueue,pubQueue,otaQueue;
extern EventGroupHandle_t wifi_event_group;
extern const int CONNECTED_BIT;

void mqtt_user_init(void);

#ifdef __cplusplus
}
#endif




#endif /* MAIN_MQTT_USER_H_ */
