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

#include "mqtt_client.h"


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

static esp_wps_config_t config = WPS_CONFIG_INIT_DEFAULT(WPS_TEST_MODE);

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

#define MQTT_PUB_MESSAGE_FORMAT "esp32/%02X%02X/%s"

static const char *TAG = "MQTTS";
static const char* chanNames[] = {"channel0","channel1","channel2","channel3"};
static const int maxChanIndex = sizeof(chanNames)/sizeof(chanNames[0]);

static char mqtt_sub_msg[32] = {0};
static char mqtt_pub_msg[32] = {0};

/* subqueue for handling messages to gpio,
 * pubQueue for handling messages from gpio (autoOff) to mqtt */
QueueHandle_t subQueue,pubQueue;

/* FreeRTOS event group to signal when we are connected & ready to make a request */
EventGroupHandle_t wifi_event_group;
esp_mqtt_client_handle_t client = NULL;

static void mqtt_message_handler(esp_mqtt_event_handle_t event) ;

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            /* send status of all avail channels */
			queueData_t data = { 0, mStatus };
			for (int i = 0; i < maxChanIndex; i++) {
				data.chan = i;
				if ( xQueueSend(subQueue, (void * ) &data,
						(TickType_t ) 10) != pdPASS) {
					// Failed to post the message, even after 10 ticks.
					ESP_LOGI(TAG, "subqueue post failure");
				}
			}
			msg_id = esp_mqtt_client_subscribe(client, mqtt_sub_msg, 1);
			ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            mqtt_message_handler(event);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
    }
    return ESP_OK;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static void mqtt_message_handler(esp_mqtt_event_handle_t event) {

	const char *pTopic;
	int chan = -1;
	gpio_task_mode_t func = mStatus;

	ESP_LOGI(TAG, "Topic received!: %.*s %.*s", event->topic_len,
			 event->topic, event->data_len,	event->data);

	if (event->topic_len > strlen(mqtt_sub_msg)) {
		pTopic = &event->topic[strlen(mqtt_sub_msg)-1];

		ESP_LOGI(TAG, "%.*s", event->topic_len - strlen(mqtt_sub_msg) + 1, pTopic);

		if (*pTopic != 0) {
			chan = -1;
			for (int i = 0; i < maxChanIndex; i++) {
				/* add /control/channelName to check for messages */
				char buf[32];
				snprintf(buf, sizeof(buf), "%s%s", "/control/", chanNames[i]);

				if (strncmp(pTopic, buf, event->topic_len - strlen(mqtt_sub_msg) + 1) == 0) {
					chan = i;
					ESP_LOGI(TAG,"channel :%d found", i);
					break;
				}
			}

			if (event->data_len == 0) {
				func = mStatus;
			} else if (strncmp((const char*) event->data, "on",
					event->data_len) == 0) {
				func = mOn;
			} else if (strncmp((const char*) event->data, "off",
					event->data_len) == 0) {
				func = mOff;
			} else {
				chan = -1;
			}

			if (chan != -1) {
				queueData_t data = { chan, func };
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
static esp_err_t event_handler(void *ctx, system_event_t *event) {
	switch (event->event_id) {
	case SYSTEM_EVENT_STA_START:
		esp_wifi_connect();
		break;
	case SYSTEM_EVENT_STA_GOT_IP:
		xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		/* This is a workaround as ESP32 WiFi libs don't currently
		 auto-reassociate. */
		esp_wifi_connect();
		xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
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

	const TickType_t xTicksToWait = 10000 / portTICK_PERIOD_MS;

	tcpip_adapter_init();
	wifi_event_group = xEventGroupCreate();

	tcpip_adapter_init();

	ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

	do {
		wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
		ESP_ERROR_CHECK(esp_wifi_init(&cfg));

		ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));

		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
		ESP_ERROR_CHECK(esp_wifi_start());

		ESP_LOGI(TAG, "Waiting for wifi");
		xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true,
				xTicksToWait);

	} while (((xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT) == 0)&&(gpio_get_level(0) != 0));
}


void mqtt_task(void *pvParameters) {
	int msg_id;
	while (1) {
		// check if we have something to do
		queueData_t rxData = { 0 };

		if ((client != NULL)
				&& (xQueueReceive(pubQueue, &(rxData), (TickType_t ) 10))) {

			ESP_LOGI(TAG, "pubQueue work");

			int chan = rxData.chan;

			if (chan != -1 && (chan < maxChanIndex)) {
				char buf[255] = { 0 };
				char payload[16] = { 0 };

				snprintf(buf, sizeof(buf), "%s%s", mqtt_pub_msg, chanNames[chan]);
				snprintf(payload, sizeof(payload), "%s",
						rxData.mode == mOn ? "son" : "soff");

				ESP_LOGI(TAG, "publish %.*s : %.*s", strlen(buf), buf,
						strlen(payload), payload);

				msg_id = esp_mqtt_client_publish(client, buf, payload,
						strlen(payload) + 1, 1, 0);
				ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

			}
		}
		vTaskDelay(10);
		taskYIELD();
	}
	vTaskDelete(NULL);
}

static void mqtt_app_start(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .uri = "mqtt://raspberrypi.fritz.box:1883",
        .event_handle = mqtt_event_handler,
        // .user_context = (void *)your_context
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);
}


void app_main() {

	/* Initialize NVS — it is used to store PHY calibration data */
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	uint8_t mac[6] = {0};
	esp_base_mac_addr_get(mac);
	snprintf(mqtt_sub_msg, sizeof(mqtt_sub_msg), MQTT_PUB_MESSAGE_FORMAT, mac[1],mac[0],"#");
	snprintf(mqtt_pub_msg, sizeof(mqtt_pub_msg), MQTT_PUB_MESSAGE_FORMAT, mac[1],mac[0], "state/");

	ESP_LOGI(TAG, "sub: %s, pub: %s", mqtt_sub_msg, mqtt_pub_msg);

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

	xTaskCreate(&mqtt_task, "mqtt_task", 2*8192, NULL, 5, NULL);

	gpio_task_setup();
	mqtt_app_start();

	while (1) {
		vTaskDelay(100 / portTICK_RATE_MS);
		if (gpio_get_level(0) == 0) {
			ESP_LOGI(TAG, "start wps...");

			ESP_ERROR_CHECK(esp_wifi_wps_enable(&config));
			ESP_ERROR_CHECK(esp_wifi_wps_start(0));
		}
	}
}
