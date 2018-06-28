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

#include "gpioTask.h"
#include "mqtt_user.h"

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
#define TAG "MAIN"

/* subqueue for handling messages to gpio,
 * pubQueue for handling messages from gpio (autoOff) to mqtt */
QueueHandle_t subQueue,pubQueue;

/* FreeRTOS event group to signal when we are connected & ready to make a request */
EventGroupHandle_t wifi_event_group;

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


	gpio_task_setup();
	mqtt_user_init();

	while (1) {
		vTaskDelay(100 / portTICK_RATE_MS);
		if (gpio_get_level(0) == 0) {
			ESP_LOGI(TAG, "start wps...");

			ESP_ERROR_CHECK(esp_wifi_wps_enable(&config));
			ESP_ERROR_CHECK(esp_wifi_wps_start(0));
		}
	}
}
