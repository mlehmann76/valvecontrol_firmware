/* MQTT Example using mbedTLS sockets
 *
 * Contacts the howsmyssl.com API via TLS v1.2 and reads a JSON
 * response.
 *
 * Adapted from the ssl_client1 example in mbedtls.
 *
 * Original Copyright (C) 2006-2016, ARM Limited, All Rights Reserved, Apache 2.0 License.
 * Additions Copyright (C) Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD, Apache 2.0 License.
 * Additions Copyright (C) Copyright 2017 pcbreflux, Apache 2.0 License.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <string.h>
#include <stdlib.h>

#include "sdkconfig.h"

#include "MQTTClient.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_wps.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_task_wdt.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "mbedtls/platform.h"
#include "mbedtls/net.h"
#include "mbedtls/debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"

#include "driver/gpio.h"

#include "gpioTask.h"

/* The examples use simple WiFi configuration that you can set via
 'make menuconfig'.

 */
#if CONFIG_EXAMPLE_WPS_TYPE_PBC
#define WPS_TEST_MODE WPS_TYPE_PBC
#elif CONFIG_EXAMPLE_WPS_TYPE_PIN
#define WPS_TEST_MODE WPS_TYPE_PIN
#else
#define WPS_TEST_MODE WPS_TYPE_DISABLE
#endif /*CONFIG_EXAMPLE_WPS_TYPE_PBC*/

#ifndef PIN2STR
#define PIN2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5], (a)[6], (a)[7]
#define PINSTR "%c%c%c%c%c%c%c%c"
#endif

#define LED_GPIO_OUTPUT    27
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<LED_GPIO_OUTPUT))

static esp_wps_config_t config =
				WPS_CONFIG_INIT_DEFAULT(WPS_TEST_MODE);

/* The event group allows multiple bits for each event,
 but we only care about one event - are we connected
 to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

/* Constants that aren't configurable in menuconfig */
#define MQTT_SERVER "raspberrypi"
#define MQTT_USER "sensor1"
#define MQTT_PASS "sensor1"
#define MQTT_PORT 8883
#define MQTT_BUF_SIZE 1000
#define MQTT_WEBSOCKET 0 // 0=no 1=yes
#define MQTT_SUB_MESSAGE "esp32/control1/#"
#define MQTT_PUB_MESSAGE "esp32/control1/"

static unsigned char mqtt_sendBuf[MQTT_BUF_SIZE];
static unsigned char mqtt_readBuf[MQTT_BUF_SIZE];

static const char *TAG = "MQTTS";
static const char* chanNames[] = {"channel0","channel1","channel2","channel3"};

/* subqueue for handling messages to gpio,
 * pubQueue for handling messages from gpio (autoOff) to mqtt */
QueueHandle_t subQueue,pubQueue;

/* FreeRTOS event group to signal when we are connected & ready to make a request */
EventGroupHandle_t wifi_event_group;

static esp_err_t event_handler(void *ctx, system_event_t *event) {
	switch (event->event_id) {
	case SYSTEM_EVENT_STA_START:
		esp_wifi_connect();
		break;
	case SYSTEM_EVENT_STA_GOT_IP:
		xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
		gpio_set_level(LED_GPIO_OUTPUT, 1);
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		/* This is a workaround as ESP32 WiFi libs don't currently
		 auto-reassociate. */
		esp_wifi_connect();
		xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
		gpio_set_level(LED_GPIO_OUTPUT, 0);
		break;
	case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
		/*point: the function esp_wifi_wps_start() only get ssid & password
		 * so call the function esp_wifi_connect() here
		 * */
		ESP_LOGI(TAG, "SYSTEM_EVENT_STA_WPS_ER_SUCCESS")		;
		ESP_ERROR_CHECK(esp_wifi_wps_disable())		;
		ESP_ERROR_CHECK(esp_wifi_connect())		;
		break;
	case SYSTEM_EVENT_STA_WPS_ER_FAILED:
		ESP_LOGI(TAG, "SYSTEM_EVENT_STA_WPS_ER_FAILED")		;
		ESP_ERROR_CHECK(esp_wifi_wps_disable())		;
		ESP_ERROR_CHECK(esp_wifi_wps_enable(&config))		;
		ESP_ERROR_CHECK(esp_wifi_wps_start(0))		;
		break;
	case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
		ESP_LOGI(TAG, "SYSTEM_EVENT_STA_WPS_ER_TIMEOUT")		;
		ESP_ERROR_CHECK(esp_wifi_wps_disable())		;
		ESP_ERROR_CHECK(esp_wifi_wps_enable(&config))		;
		ESP_ERROR_CHECK(esp_wifi_wps_start(0))		;
		break;
	case SYSTEM_EVENT_STA_WPS_ER_PIN:
		ESP_LOGI(TAG, "SYSTEM_EVENT_STA_WPS_ER_PIN")		;
		/*show the PIN code here*/
		ESP_LOGI(TAG, "WPS_PIN = "PINSTR,
				PIN2STR(event->event_info.sta_er_pin.pin_code))		;
		break;
	default:
		break;
	}
	return ESP_OK;
}

static void initialise_wifi(void) {
	tcpip_adapter_init();
	wifi_event_group = xEventGroupCreate();

	tcpip_adapter_init();

	ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT()	;
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_start());
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static void mqtt_message_handler(MessageData *md) {

	const char *pTopic;
	int chan = -1;
	gpio_task_mode_t func = mStatus;

	ESP_LOGI(TAG, "Topic received!: %.*s %.*s", md->topicName->lenstring.len,
			md->topicName->lenstring.data, md->message->payloadlen,
			(char* )md->message->payload);

	if (md->topicName->lenstring.len > strlen(MQTT_SUB_MESSAGE)) {
		pTopic = &md->topicName->lenstring.data[strlen(MQTT_SUB_MESSAGE)-1];

		ESP_LOGI(TAG, "%.*s", md->topicName->lenstring.len - strlen(MQTT_SUB_MESSAGE) + 1, pTopic);

		if (*pTopic != 0) {
			chan = -1;
			for (int i = 0; i < (sizeof(chanNames) / sizeof(chanNames[0]));
					i++) {
				if (strncmp(pTopic, chanNames[i], md->topicName->lenstring.len - strlen(MQTT_SUB_MESSAGE) + 1) == 0) {
					chan = i;
					ESP_LOGI(TAG,"channel :%d found", i);
					break;
				}
			}

			if (md->message->payloadlen == 0) {
				func = mStatus;
			} else if (strncmp((const char*) md->message->payload, "on",
					md->message->payloadlen) == 0) {
				func = mOn;
			} else if (strncmp((const char*) md->message->payload, "off",
					md->message->payloadlen) == 0) {
				func = mOff;
			} else {
				func = mStatus;
			}

			if (chan != -1) {
				queueData_t data = { 1 << chan, func };
				// available if necessary.
				if ( xQueueSend(subQueue, (void * ) &data,
						(TickType_t ) 10) != pdPASS) {
					// Failed to post the message, even after 10 ticks.
					ESP_LOGI(TAG, "subqueue post failure");
				}
			}
		}
	}
}

#pragma GCC diagnostic pop

int chan2Index(const queueData_t* rxData,
		int chan) {
	for (int i = 0; i < (sizeof(chanNames) / sizeof(chanNames[0])); i++) {
		if ((rxData->chan & (1 << i)) != 0) {
			chan = i;
			break;
		}
	}
	return chan;
}

void mqtt_task(void *pvParameters) {
	int ret;
	Network network;

	while (1) {
		ESP_LOGD(TAG, "Wait for WiFi ...");
		/* Wait for the callback to set the CONNECTED_BIT in the
		 event group.
		 */
		xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
		false, true, portMAX_DELAY);
		ESP_LOGD(TAG, "Connected to AP");

		ESP_LOGD(TAG, "Start MQTT Task ...");
		ESP_LOGI(TAG, "Free Heap...%d", esp_get_free_heap_size())

		MQTTClient client;
		NetworkInit(&network);
		network.websocket = MQTT_WEBSOCKET;

		ESP_LOGD(TAG, "NetworkConnect %s:%d ...", MQTT_SERVER, MQTT_PORT);
		ret = NetworkConnect(&network, MQTT_SERVER, MQTT_PORT);
		if (ret != 0) {
			ESP_LOGI(TAG, "NetworkConnect not SUCCESS: %d", ret);
			goto exit;
		}
		ESP_LOGD(TAG, "MQTTClientInit  ...");
		MQTTClientInit(&client, &network, 2000,            // command_timeout_ms
				mqtt_sendBuf,         //sendbuf,
				MQTT_BUF_SIZE, //sendbuf_size,
				mqtt_readBuf,         //readbuf,
				MQTT_BUF_SIZE  //readbuf_size
				);

		char buf[30];
		MQTTString clientId = MQTTString_initializer;
#if defined(MBEDTLS_MQTT_DEBUG)
		sprintf(buf, "ESP32MQTT");
#else
		sprintf(buf, "ESP32MQTT%08X", esp_random());
#endif
		ESP_LOGI(TAG, "MQTTClientInit  %s", buf);
		clientId.cstring = buf;

		MQTTString username = MQTTString_initializer;
		username.cstring = MQTT_USER;

		MQTTString password = MQTTString_initializer;
		password.cstring = MQTT_PASS;

		MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
		data.clientID = clientId;
		data.willFlag = 0;
		data.MQTTVersion = 4; // 3 = 3.1 4 = 3.1.1
		data.keepAliveInterval = 60;
		data.cleansession = 1;
		data.username = username;
		data.password = password;

		ESP_LOGI(TAG, "MQTTConnect  ...");
		ret = MQTTConnect(&client, &data);
		if (ret != SUCCESS) {
			ESP_LOGI(TAG, "MQTTConnect not SUCCESS: %d", ret);
			goto exit;
		}

		ESP_LOGI(TAG, "MQTTSubscribe  ...");
		ret = MQTTSubscribe(&client, MQTT_SUB_MESSAGE, QOS0,
				mqtt_message_handler);
		if (ret != SUCCESS) {
			ESP_LOGI(TAG, "MQTTSubscribe: %d", ret);
			goto exit;
		}

		ESP_LOGI(TAG, "MQTTYield  ...");
		while (1) {
			// check if we have something to do
			queueData_t rxData = { 0 };

			if (xQueueReceive(pubQueue, &(rxData), (TickType_t ) 10)) {

				ESP_LOGI(TAG, "pubQueue work");

				int chan = chan2Index(&rxData, -1);

				if (chan != -1) {
					char buf[255] = { 0 };
					char payload[16] = { 0 };

					snprintf(buf, sizeof(buf), "%s%s", MQTT_PUB_MESSAGE, chanNames[chan]);
					snprintf(payload, sizeof(payload), "%s", rxData.mode == mOn ? "on": "off");

					ESP_LOGI(TAG, "publish %.*s : %.*s", strlen(buf), buf, strlen(payload), payload);
					//
					MQTTMessage message;
					message.qos = QOS1;
					message.retained = 0;
					message.dup = 0;
					message.payload = payload;
					message.payloadlen = strlen(payload)+1;

					ret = MQTTPublish(&client, buf, &message);

					if (ret != SUCCESS) {
						ESP_LOGI(TAG, "MQTTPublish: %d", ret);
						goto exit;
					}
				}
			} else {

				ret = MQTTYield(&client, 1000);

				if (ret != SUCCESS) {
					ESP_LOGI(TAG, "MQTTYield: %d", ret);
					goto exit;
				}
			}

		}
		exit: MQTTDisconnect(&client);
		NetworkDisconnect(&network);
		ESP_LOGI(TAG, "Starting again!");
	}
	vTaskDelete(NULL);
}

void app_main() {

	/* Initialize NVS — it is used to store PHY calibration data */
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	initialise_wifi();


	// Create a queue capable of containing 10 uint32_t values.
	subQueue = xQueueCreate(10, sizeof(queueData_t));
	if (subQueue == 0) {
		// Queue was not created and must not be used.
		ESP_LOGI(TAG, "subqueue init failure");
	}

	pubQueue = xQueueCreate(10, sizeof(queueData_t));
	if (pubQueue == 0) {
		// Queue was not created and must not be used.
		ESP_LOGI(TAG, "pubqueue init failure");
	}

	xTaskCreate(&mqtt_task, "mqtt_task", 2*8192, NULL, 2, NULL);
	xTaskCreate(&gpio_task, "gpio_task", 2048, NULL, 1, NULL);

	while (1) {
		vTaskDelay(100 / portTICK_RATE_MS);
		if (gpio_get_level(0) == 0) {
			ESP_LOGI(TAG, "start wps...");

			ESP_ERROR_CHECK(esp_wifi_wps_enable(&config));
			ESP_ERROR_CHECK(esp_wifi_wps_start(0));
		}
	}
}
