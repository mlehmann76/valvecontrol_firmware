/*
 * mqtt_user_ota.h
 *
 *  Created on: 29.06.2018
 *      Author: marco
 */

#ifndef MQTT_USER_OTA_H_
#define MQTT_USER_OTA_H_

#define DECODEBUFSIZE 2048
#define DATA_RCV (1<<0)
#define UPDATE_REQ (1<<1)

typedef enum {
	OTA_IDLE, OTA_START, OTA_DATA, OTA_FINISH, OTA_ERROR
} ota_state_t;

typedef struct {
	uint32_t decodePos;
	uint32_t len;
	uint32_t sumLen;
	uint8_t buffer[DECODEBUFSIZE];
	uint8_t decoded[DECODEBUFSIZE];
} decode_t;

typedef struct {
	uint8_t md5[16];
	size_t len;
} md5_update_t;


int handleOtaMessage(esp_mqtt_event_handle_t event);
void handleOta();
void mqtt_ota_task(void *pvParameters);

#endif /* MQTT_USER_OTA_H_ */
