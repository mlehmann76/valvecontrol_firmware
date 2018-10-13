/*
 * mqtt_user_ota.h
 *
 *  Created on: 29.06.2018
 *      Author: marco
 */

#ifndef MQTT_USER_OTA_H_
#define MQTT_USER_OTA_H_

void handleOtaMessage(esp_mqtt_event_handle_t event);
void handleOta();


#endif /* MQTT_USER_OTA_H_ */
