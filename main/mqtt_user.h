/*
 * mqtt_client.h
 *
 *  Created on: 28.06.2018
 *      Author: marco
 */

#ifndef MAIN_MQTT_USER_H_
#define MAIN_MQTT_USER_H_

/* Constants that aren't configurable in menuconfig */
#define MQTT_SERVER "mqtt://raspberrypi.fritz.box:1883"
#define MQTT_USER "sensor1"
#define MQTT_PASS "sensor1"
#define MQTT_BUF_SIZE 1000
#define MQTT_WEBSOCKET 0 // 0=no 1=yes

#define MQTT_PUB_MESSAGE_FORMAT "esp32/%02X%02X/%s"


void mqtt_user_init(void);




#endif /* MAIN_MQTT_USER_H_ */
