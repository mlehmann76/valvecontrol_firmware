#include <string.h>
#include <stdlib.h>

//#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wps.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "esp_task_wdt.h"
#include "esp_http_server.h"
#include "cJSON.h"

#include "mqtt_config.h"
#include "mqtt_client.h"
#include "mqtt_user.h"
#include "mqtt_user_ota.h"

#include "http_config_server.h"
#include "config_user.h"
#include "controlTask.h"
#include "status.h"
#include "sht1x.h"
#include "sntp.h"
#include "jsonconfig.h"


#if CONFIG_EXAMPLE_WPS_TYPE_PBC
#define WPS_TEST_MODE WPS_TYPE_PBC
#elif CONFIG_EXAMPLE_WPS_TYPE_PIN
#define WPS_TEST_MODE WPS_TYPE_PIN
#else
#define WPS_TEST_MODE WPS_TYPE_PBC
#endif /*CONFIG_EXAMPLE_WPS_TYPE_PBC*/

#ifndef PIN2STR
#define PIN2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5], (a)[6], (a)[7]
#define PINSTR "%c%c%c%c%c%c%c%c"
#endif

static esp_wps_config_t config = WPS_CONFIG_INIT_DEFAULT(WPS_TEST_MODE);
static bool enableWPS = false;
static time_t lastMqttSeen;

/* The event group allows multiple bits for each event,
 but we only care about one event - are we connected
 to the AP with an IP? */

#define TAG "MAIN"

/* FreeRTOS event group to signal when we are connected & ready to make a request */
EventGroupHandle_t main_event_group;
httpd_handle_t server_handle;

#pragma GCC diagnostic pop
static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
	httpd_handle_t *server = (httpd_handle_t*) arg;
	enableWPS = false;

	switch (event_id) {
	case WIFI_EVENT_WIFI_READY: /**< ESP32 WiFi ready */
		break;
	case WIFI_EVENT_SCAN_DONE: /**< ESP32 finish scanning AP */
		break;
	case WIFI_EVENT_STA_START: /**< ESP32 station start */
		switch (esp_wifi_connect()) {
		case ESP_OK:
			ESP_LOGI(TAG, "connected successfully");
			break;

		case ESP_ERR_WIFI_NOT_INIT:
			ESP_LOGE(TAG, "WiFi is not initialized by eps_wifi_init");
			break;

		case ESP_ERR_WIFI_NOT_STARTED:
			ESP_LOGE(TAG, "WiFi is not started by esp_wifi_start");
			break;

		case ESP_ERR_WIFI_CONN:
			ESP_LOGE(TAG, "WiFi internal error, station or soft-AP control block wrong");
			break;

		case ESP_ERR_WIFI_SSID:
			ESP_LOGE(TAG, "SSID of AP which station connects is invalid");
			enableWPS = true;
			break;

		default:
			ESP_LOGE(TAG, "Unknown return code");
			break;
		}
		break;
		break;
	case WIFI_EVENT_STA_STOP: /**< ESP32 station stop */
		break;
	case WIFI_EVENT_STA_CONNECTED: /**< ESP32 station connected to AP */
		break;
	case WIFI_EVENT_STA_DISCONNECTED: /**< ESP32 station disconnected from AP */
		/* This is a workaround as ESP32 WiFi libs don't currently
		 auto-reassociate. */
		esp_wifi_connect();
		xEventGroupClearBits(main_event_group, CONNECTED_BIT);
		/* Stop the web server */
		if (*server) {
			stop_webserver(*server);
			*server = NULL;
		}
		mqtt_disconnect();
		break;
		break;
	case WIFI_EVENT_STA_AUTHMODE_CHANGE: /**< the auth mode of AP connected by ESP32 station changed */
		break;

	case WIFI_EVENT_STA_WPS_ER_SUCCESS: /**< ESP32 station wps succeeds in enrollee mode */
		/*point: the function esp_wifi_wps_start() only get ssid & password
		 * so call the function esp_wifi_connect() here
		 * */
		ESP_LOGI(TAG, "SYSTEM_EVENT_STA_WPS_ER_SUCCESS");
		ESP_ERROR_CHECK(esp_wifi_wps_disable())	;
		ESP_ERROR_CHECK(esp_wifi_connect())	;

		break;
	case WIFI_EVENT_STA_WPS_ER_FAILED: /**< ESP32 station wps fails in enrollee mode */
		ESP_LOGI(TAG, "SYSTEM_EVENT_STA_WPS_ER_FAILED");
		ESP_ERROR_CHECK(esp_wifi_wps_disable());
		ESP_ERROR_CHECK(esp_wifi_wps_enable(&config));
		ESP_ERROR_CHECK(esp_wifi_wps_start(0));
		break;
	case WIFI_EVENT_STA_WPS_ER_TIMEOUT: /**< ESP32 station wps timeout in enrollee mode */
		ESP_LOGI(TAG, "SYSTEM_EVENT_STA_WPS_ER_TIMEOUT");
		ESP_ERROR_CHECK(esp_wifi_wps_disable());
		ESP_ERROR_CHECK(esp_wifi_wps_enable(&config));
		ESP_ERROR_CHECK(esp_wifi_wps_start(0));
		break;
	case WIFI_EVENT_STA_WPS_ER_PIN: /**< ESP32 station wps pin code in enrollee mode */
		ESP_LOGI(TAG, "SYSTEM_EVENT_STA_WPS_ER_PIN");
		/*show the PIN code here*/
		ESP_LOGI(TAG, "WPS_PIN = "PINSTR, PIN2STR(((wifi_event_sta_wps_er_pin_t*)event_data)->pin_code));
		break;
	case WIFI_EVENT_STA_WPS_ER_PBC_OVERLAP: /**< ESP32 station wps overlap in enrollee mode */

		break;
	case WIFI_EVENT_AP_START: /**< ESP32 soft-AP start */
		break;
	case WIFI_EVENT_AP_STOP: /**< ESP32 soft-AP stop */
		break;
	case WIFI_EVENT_AP_STACONNECTED: /**< a station connected to ESP32 soft-AP */
		break;
	case WIFI_EVENT_AP_STADISCONNECTED: /**< a station disconnected from ESP32 soft-AP */
		break;
	case WIFI_EVENT_AP_PROBEREQRECVED: /**< Receive probe request packet in soft-AP interface */
		break;
	}
}
/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
	httpd_handle_t *server = (httpd_handle_t*) arg;
	ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
	switch (event_id) {
	case IP_EVENT_STA_GOT_IP:
		ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		xEventGroupSetBits(main_event_group, CONNECTED_BIT);
		if (status_event_group != NULL) {
			xEventGroupSetBits(status_event_group, STATUS_EVENT_FIRMWARE | STATUS_EVENT_HARDWARE);
		}

		/* Start the web server */
		if (*server == NULL) {
			*server = start_webserver();
		}
		mqtt_connect();
		break;
	case IP_EVENT_STA_LOST_IP:
		break;
	}
}

void wifi_init_sta(void *param) {
	enum state_t {
		w_disconnected, w_connected, w_wps_enable, w_wps
	} w_state = w_disconnected;
	int timeout = 0;

	main_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta();
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT()	;

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;

	ESP_ERROR_CHECK(
			esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, &server_handle, &instance_any_id));
	ESP_ERROR_CHECK(
			esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &got_ip_event_handler, &server_handle, &instance_got_ip));

	while (1) {
		bool isConnected = (xEventGroupGetBits(main_event_group) & CONNECTED_BIT) != 0;
		bool isMQTTConnected = (xEventGroupGetBits(main_event_group) & MQTT_CONNECTED_BIT) != 0;

		switch (w_state) {
		case w_disconnected:
			ESP_ERROR_CHECK(esp_wifi_init(&cfg));
			ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
			ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
			ESP_ERROR_CHECK(esp_wifi_start());

			if (!enableWPS) {
				timeout = 30;
				w_state = w_connected;
			} else {
				timeout = 5;
				w_state = w_wps_enable;
			}
			break;

		case w_connected:
			if (!isConnected) {
				if (timeout == 0) {

					ESP_LOGI(TAG, "wifi_init_sta stop sta");
					ESP_ERROR_CHECK(esp_wifi_stop());
					ESP_LOGI(TAG, "wifi_init_sta deinit sta");
					ESP_ERROR_CHECK(esp_wifi_deinit());
					w_state = w_disconnected;
				} else {
					vTaskDelay(1000 / portTICK_PERIOD_MS);
					timeout--;
				}
			} else {
				timeout = 30;
				if (isMQTTConnected) {
					time(&lastMqttSeen);
				}
			}
			break;

		case w_wps_enable:
			if (!isConnected) {
				if (timeout == 0) {
					ESP_LOGI(TAG, "start wps...");
					ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_wps_enable(&config));
					ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_wps_start(0));
					enableWPS = false;
					timeout = 60;
					w_state = w_wps;
				} else {
					vTaskDelay(1000 / portTICK_PERIOD_MS);
					timeout--;
				}
			}
			break;

		case w_wps:
			if (isConnected) {
				w_state = w_connected;
			} else {
				vTaskDelay(1000 / portTICK_PERIOD_MS);
				timeout--;
				if (timeout == 0) {
					w_state = w_disconnected;
				}
			}
			break;
		}

		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
}

void spiffsInit(void) {
	ESP_LOGI(TAG, "Initializing SPIFFS");

	esp_vfs_spiffs_conf_t conf = { .base_path = "/spiffs", .partition_label = NULL, .max_files = 5,
			.format_if_mount_failed = false };

	// Use settings defined above to initialize and mount SPIFFS filesystem.
	// Note: esp_vfs_spiffs_register is an all-in-one convenience function.
	esp_err_t ret = esp_vfs_spiffs_register(&conf);

	if (ret != ESP_OK) {
		if (ret == ESP_FAIL) {
			ESP_LOGE(TAG, "Failed to mount or format filesystem");
		} else if (ret == ESP_ERR_NOT_FOUND) {
			ESP_LOGE(TAG, "Failed to find SPIFFS partition");
		} else {
			ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
		}
		return;
	}

	size_t total = 0, used = 0;
	ret = esp_spiffs_info(NULL, &total, &used);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
	} else {
		ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
	}
}

void app_main() {

	esp_log_level_set("phy_init", ESP_LOG_ERROR);
	esp_log_level_set("MQTT_CLIENT", ESP_LOG_ERROR);
	esp_log_level_set("HTTP", ESP_LOG_VERBOSE);

	static httpd_handle_t server = NULL;

	/* Initialize NVS — it is used to store PHY calibration data */
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
	xTaskCreate(wifi_init_sta, "wifi init task", 4096, &server, 10, NULL);

	configInit();
	spiffsInit();
	sntp_support();
	status_task_setup();
	gpio_task_setup();
	setupSHT1xTask();
	mqtt_config_init();
	mqtt_user_init();
	mqtt_user_addHandler(&controlHandler);

	ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());

	size_t heapFree = esp_get_free_heap_size();
	time_t now;
	struct tm timeinfo;
	time(&lastMqttSeen);

	while (1) {

#if 0
		char strftime_buf[64];
		time(&now);
		localtime_r(&now, &timeinfo);
		//if (xEventGroupWaitBits(button_event_group, WPS_SHORT_BIT, true, true,10)) {
		/*
		 time(&now);
		 if (difftime(now,last) >=2 ) {
		 vTaskGetRunTimeStats(task_debug_buf);
		 ESP_LOGI(TAG, "%s", task_debug_buf);
		 last = now;
		 }
		 */
		if (timeinfo.tm_year > (2016 - 1900)) {
			localtime_r(&now, &timeinfo);
			strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
			ESP_LOGI(TAG, "The current date/time is: %s", strftime_buf);
			vTaskDelay(5000 / portTICK_PERIOD_MS);
		} else {
			vTaskDelay(500 / portTICK_PERIOD_MS);
		}
#else
		time(&now);
		localtime_r(&now, &timeinfo);
		if (timeinfo.tm_year > (2016 - 1900)) {
			//reboot @0:0:0 if received sntp
			if ((timeinfo.tm_hour == 0) && (timeinfo.tm_min == 0)) {
				if ((timeinfo.tm_sec == 0) || (timeinfo.tm_sec == 1)) {
					esp_restart();
				}
			}
		} else { // reboot after 1h when not connected to mqtt server
			if (difftime(now, lastMqttSeen) > 3600) {
				esp_restart();
			}
		}
		if (esp_get_free_heap_size() != heapFree) {
			heapFree = esp_get_free_heap_size();
			ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
		}
		vTaskDelay(500 / portTICK_PERIOD_MS);
#endif
	}
}
