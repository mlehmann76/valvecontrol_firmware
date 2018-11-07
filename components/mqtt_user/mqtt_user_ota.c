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
#include "esp_ota_ops.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "cJSON.h"
#include "mqtt_client.h"
#include "mqtt_user.h"
#include "mqtt_user_ota.h"
#include "mqtt_config.h"

#include "mbedtls/md5.h"
#include "base85.h"
#include "config_user.h"

#define TAG "OTA"

extern QueueHandle_t otaQueue;

static mbedtls_md5_context ctx;
static decode_t decodeCtx;
static md5_update_t md5Updata;
static QueueHandle_t dataQueue;

ota_state_t ota_state = OTA_IDLE;
SemaphoreHandle_t xSemaphore = NULL;
messageHandler_t otaHandler = {.pUserctx = NULL, .onMessage = handleOtaMessage};

static void __attribute__((noreturn)) task_fatal_error()
{
    ESP_LOGE(TAG, "Exiting task due to fatal error...");
    (void)vTaskDelete(NULL);

    while (1) {
        ;
    }
}

static void b85DecodeInit(decode_t *src){
	if (src != NULL) {
		memset(src->buffer, 0, DECODEBUFSIZE);
		memset(src->decoded, 0, DECODEBUFSIZE);
		src->decodePos = 0;
		src->sumLen = 0;
		src->len = 0;
	}
}

static int b85Decode(const uint8_t *src, size_t len, decode_t *dest) {
	dest->len = 0;
	if (dest != NULL) {
		int decLen = ((len + dest->decodePos) / 5) * 5;
		int decRest = (len + dest->decodePos) % 5;
		if ((len < (DECODEBUFSIZE) - dest->decodePos)) {
			//copy data to buffer
			memcpy(&dest->buffer[dest->decodePos], src, len);
			//decode
			int ret = decode_85((char*)dest->decoded, (const char*)dest->buffer, decLen / 5 * 4);

			if (ret == 0) {
				//copy rest to buffer
				for (int i = 0; i < decRest; i++) {
					dest->buffer[i] = dest->buffer[i + decLen];
				}
				dest->decodePos = decRest;
				dest->len = decLen / 5 * 4;
				dest->sumLen += decLen / 5 * 4;
				ESP_LOGV(TAG, "decode %d", dest->len);
			} else {
				ESP_LOGE(TAG, "decode error %d", ret);
			}
		}
	}

	return dest->len;
}

int handleOtaMessage(pCtx_t p, esp_mqtt_event_handle_t event) {
	int ret = 0;
	if (xSemaphore != NULL) {
		const char* pTopic =
				event->topic_len >= strlen(getSubMsg()) ?
						&event->topic[strlen(getSubMsg()) - 1] : "";

		if ((ota_state != OTA_IDLE)) {
			if (((event->topic_len == 0)
					|| (strcmp(pTopic, "/ota/$implementation/binary") == 0))) {

				//block till data was processed by ota task
				const TickType_t xTicksToWait = 10000 / portTICK_PERIOD_MS;
				if (xQueueSend(dataQueue, (void * ) &decodeCtx.len,	xTicksToWait) != pdPASS) {
					// Failed to post the message, even after 100 ticks.
					ESP_LOGI(TAG, "dataqueue post failure");
				}
				//ESP_LOGI(TAG, "handle ota message, len (%d) %.*s", event->data_len, event->data_len, event->data);{
				if ( xSemaphoreTake( xSemaphore, xTicksToWait) == pdTRUE) {

					int len = b85Decode((uint8_t*) event->data, event->data_len,&decodeCtx);
					if (len > 0) {
						mbedtls_md5_update(&ctx, decodeCtx.decoded, len);
					}
					LED_TOGGLE();
					ret = 1;
					xSemaphoreGive(xSemaphore);
				}
			}
		}
	}
	return ret;
}

void mqtt_ota_task(void *pvParameters) {
	static uint32_t last = 0, temp;
    esp_err_t err;
    /* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
    esp_ota_handle_t update_handle = 0 ;
    const esp_partition_t *update_partition = NULL;
    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

	xSemaphore = xSemaphoreCreateBinary();

	if( xSemaphore == NULL ) {
		ESP_LOGE(TAG, "error creating semaphore");
	} else {
		xSemaphoreGive(xSemaphore);
	}

	dataQueue = xQueueCreate(1, sizeof(uint32_t));
	if (dataQueue == NULL) {
		// Queue was not created and must not be used.
		ESP_LOGI(TAG, "dataQueue init failure");
	}

	ESP_LOGI(TAG, "init md5");
	mbedtls_md5_init(&ctx);

	for (;;) {
		if (otaQueue != NULL) {
			md5_update_t rxData = { 0 };
			// Receive a message on the created queue.  Block for 10 ticks if a
			// message is not immediately available.
			const TickType_t xTicksToWait = 10 / portTICK_PERIOD_MS;
			if (xQueueReceive(otaQueue, &(rxData), xTicksToWait)) {
				md5Updata = rxData;
				ota_state = OTA_START;
			}
		}
		if (xSemaphore != NULL) {
			struct timeval tv;
			gettimeofday(&tv, NULL);

			uint32_t now = tv.tv_sec;
			unsigned char output[16];

			const TickType_t xTicksToWait = 10 / portTICK_PERIOD_MS;
			if ( xSemaphoreTake( xSemaphore, xTicksToWait ) == pdTRUE) {
				switch (ota_state) {
				case OTA_IDLE:
					break;

				case OTA_START:
					if (configured != running) {
						ESP_LOGW(TAG,
								"Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
								configured->address, running->address);
						ESP_LOGW(TAG,
								"(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
					}
					ESP_LOGI(TAG,
							"Running partition type %d subtype %d (offset 0x%08x)",
							running->type, running->subtype, running->address);

					update_partition = esp_ota_get_next_update_partition(NULL);

					ESP_LOGI(TAG,
							"Writing to partition subtype %d at offset 0x%x",
							update_partition->subtype,
							update_partition->address);

					assert(update_partition != NULL);

					err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN,
							&update_handle);

					if (err != ESP_OK) {
						ESP_LOGE(TAG, "esp_ota_begin failed (%s)",
								esp_err_to_name(err));
						task_fatal_error();
					}

					ESP_LOGI(TAG, "esp_ota_begin succeeded");

					mbedtls_md5_init(&ctx);
					mbedtls_md5_starts(&ctx);
					b85DecodeInit(&decodeCtx);
					last = now;
					ota_state = OTA_DATA;
					break;

				case OTA_DATA:
					if (xQueueReceive(dataQueue, &(temp), (TickType_t ) 1)) {
						last = now;
						if (decodeCtx.len > 0) {
							err = esp_ota_write(update_handle,
									(const void *) decodeCtx.decoded,
									decodeCtx.len);

							if (err != ESP_OK) {
								ESP_LOGE(TAG, "esp_ota_write failed (%s)",
										esp_err_to_name(err));
								task_fatal_error();
							} else {
								ESP_LOGI(TAG, "esp_ota_write %d%%", decodeCtx.sumLen*100/md5Updata.len);
							}

							if (decodeCtx.sumLen == md5Updata.len) {
								ota_state = OTA_FINISH;
							}
						} else {
							ota_state = OTA_ERROR;
						}

					} else {
						if ((now - last) > 5) { //TODO
							ota_state = OTA_FINISH;
						}
					}
					break;

				case OTA_FINISH:
					mbedtls_md5_finish(&ctx, output);
					ESP_LOGI(TAG,
							"fileSize :%d ->md5: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
							decodeCtx.sumLen, output[0], output[1], output[2],
							output[3], output[4], output[5], output[6],
							output[7], output[8], output[9], output[10],
							output[11], output[12], output[13], output[14],
							output[15]);

					if (memcmp(md5Updata.md5, output, 16) != 0) {
						ESP_LOGE(TAG, "md5 sum failed ");
					} else {
						if (esp_ota_end(update_handle) != ESP_OK) {
							ESP_LOGE(TAG, "esp_ota_end failed!");
							task_fatal_error();
						}
						err = esp_ota_set_boot_partition(update_partition);
						if (err != ESP_OK) {
							ESP_LOGE(TAG,
									"esp_ota_set_boot_partition failed (%s)!",
									esp_err_to_name(err));
							task_fatal_error();
						}
						ESP_LOGI(TAG, "Prepare to restart system!");
						esp_restart();
						return;
					}
					break;

				case OTA_ERROR:
					break;
				}
				xSemaphoreGive(xSemaphore);
			}
		}
		vTaskDelay(1);
		taskYIELD();
	}
}
