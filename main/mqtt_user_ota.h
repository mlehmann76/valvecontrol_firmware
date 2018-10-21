/*
 * mqtt_user_ota.h
 *
 *  Created on: 29.06.2018
 *      Author: marco
 */

#ifndef MQTT_USER_OTA_H_
#define MQTT_USER_OTA_H_

typedef enum {
	OTA_IDLE, OTA_DATA, OTA_FINISH
} ota_state_t;

#define DECODEBUFSIZE 2048

typedef struct {
	uint32_t decodePos;
	uint32_t sumLen;
	uint8_t buffer[DECODEBUFSIZE];
	uint8_t decoded[DECODEBUFSIZE];
} decode_t;


int handleOtaMessage(esp_mqtt_event_handle_t event);
void handleOta();
void mqtt_ota_task(void *pvParameters);
void b85DecodeInit(decode_t *src);
int b85Decode(const uint8_t *src,  size_t len, decode_t *dest);

#endif /* MQTT_USER_OTA_H_ */
