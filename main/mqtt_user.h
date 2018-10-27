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

QueueHandle_t subQueue,pubQueue,otaQueue;

void mqtt_user_init(void);

#ifdef __cplusplus
}
#endif




#endif /* MAIN_MQTT_USER_H_ */
