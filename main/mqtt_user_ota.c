/*
 * mqtt_user_ota.c
 *
 *  Created on: 29.06.2018
 *      Author: marco
 */

#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include "sdkconfig.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "mqtt_client.h"
#include "mqtt_user.h"
#include "mqtt_user_ota.h"

#include "mbedtls/md5.h"
#include "base85.h"

#define TAG "OTA"

char payload[2048];
int payload_len = 0;
mbedtls_md5_context ctx;
EventGroupHandle_t mqtt_ota_event_group;
#define DATA_RCV (1<<0)
static decode_t decodeCtx;

ota_state_t ota_state = OTA_IDLE;

void b85DecodeInit(decode_t *src){
	if (src != NULL) {
		memset(src->buffer, 0, DECODEBUFSIZE);
		memset(src->decoded, 0, DECODEBUFSIZE);
		src->decodePos = 0;
		src->sumLen = 0;
	}
}

int b85Decode(const uint8_t *src, size_t len, decode_t *dest) {
	if (dest != NULL) {
		int decLen = ((len + dest->decodePos) / 5) * 5;
		int decRest = (len + dest->decodePos) % 5;
		if ((len < (DECODEBUFSIZE) - dest->decodePos)) {
			//copy data to buffer
			memcpy(&dest->buffer[dest->decodePos], src, len);
			//decode
			ESP_LOGI(TAG, "decodeLen %d, decodeRest %d", decLen, decRest);

			int ret = decode_85((char*)dest->decoded, (const char*)dest->buffer, decLen / 5 * 4);

			if (ret != 0) {
				ESP_LOGI(TAG, "decode error %d", ret);
				return -1; //TODO
			}
			//copy rest to buffer
			for (int i = 0; i < decRest; i++) {
				dest->buffer[i] = dest->buffer[i + decLen];
			}
			dest->decodePos = decRest;
		}
		dest->sumLen += decLen / 5 * 4;
		return decLen / 5 * 4;
	} else {
		return -1;
	}
}

int handleOtaMessage(esp_mqtt_event_handle_t event) {
	int ret = 0;
	const char* pTopic =
			event->topic_len >= strlen(mqtt_sub_msg) ?
					&event->topic[strlen(mqtt_sub_msg) - 1] : "";

	if (mqtt_ota_event_group != NULL) {
		if (((event->topic_len == 0) || (strcmp(pTopic, "ota") == 0))) {
			ESP_LOGI(TAG, "handle ota message, len %d", event->data_len);

			if (event->topic_len != 0) {
				mbedtls_md5_starts(&ctx);
				b85DecodeInit(&decodeCtx);
				int len = b85Decode((uint8_t*)event->data,  event->data_len, &decodeCtx);
				if (len > 0) {
					mbedtls_md5_update(&ctx, decodeCtx.decoded, len);
				}

			} else {
				int len = b85Decode((uint8_t*)event->data,  event->data_len, &decodeCtx);
				if (len > 0) {
					mbedtls_md5_update(&ctx, decodeCtx.decoded, len);
				}
			}
			xEventGroupSetBits(mqtt_ota_event_group, DATA_RCV);
			ret = 1;
		}
	}
	return ret;
}

void handleOta() {
	static uint32_t last = 0;

	if (mqtt_ota_event_group != NULL) {
		struct timeval tv;
		gettimeofday(&tv, NULL);

		uint32_t now = tv.tv_sec;
		unsigned char output[16];

		switch (ota_state) {
		case OTA_IDLE:
			if (xEventGroupWaitBits(mqtt_ota_event_group, DATA_RCV, pdTRUE, pdFALSE, (TickType_t) 1)) {
				last = now;
				ota_state = OTA_DATA;
			}
			break;

		case OTA_DATA:
			if (xEventGroupWaitBits(mqtt_ota_event_group, DATA_RCV, pdTRUE, pdFALSE, (TickType_t) 1)) {
				last = now;
			} else {
				if ((now - last) > 2) { //TODO
					ota_state = OTA_FINISH;
				}
			}
			break;
		case OTA_FINISH:
			mbedtls_md5_finish(&ctx, output);
			ESP_LOGI(TAG,
					"fileSize :%d ->md5: %2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x",
					decodeCtx.sumLen,
					output[0], output[1], output[2], output[3], output[4],
					output[5], output[6], output[7], output[8], output[9],
					output[10], output[11], output[12], output[13], output[14],
					output[15])	;
			ota_state = OTA_IDLE;
		}
	}
}

void mqtt_ota_task(void *pvParameters) {
	mqtt_ota_event_group = xEventGroupCreate();
	ESP_LOGI(TAG, "init md5");
	mbedtls_md5_init(&ctx);
	for (;;) {
		handleOta();
		vTaskDelay(1);
		taskYIELD();
	}
}
