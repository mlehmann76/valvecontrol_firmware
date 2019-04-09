#include <string.h>
#include <stdlib.h>

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wps.h"
#include "esp_system.h"
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

static esp_wps_config_t config = WPS_CONFIG_INIT_DEFAULT(WPS_TEST_MODE);
static bool enableWPS = false;
static bool g_wifi_reconnect_flag = true;
static bool g_wifi_wps_flag = true;

/* The event group allows multiple bits for each event,
 but we only care about one event - are we connected
 to the AP with an IP? */
const int CONNECTED_BIT = 1u<<0;
#define TAG "MAIN"
//static char task_debug_buf[512];

/* FreeRTOS event group to signal when we are connected & ready to make a request */
EventGroupHandle_t wifi_event_group, button_event_group;

void activateWPS(const esp_wps_config_t* config) {
	ESP_LOGI(TAG, "start wps...");
	ESP_ERROR_CHECK(esp_wifi_wps_enable(config));
	ESP_ERROR_CHECK(esp_wifi_wps_start(0));
	g_wifi_wps_flag = true;
}

#pragma GCC diagnostic pop
static esp_err_t event_handler(void *ctx, system_event_t *event) {
	httpd_handle_t *server = (httpd_handle_t *) ctx;
	enableWPS = false;
	switch (event->event_id) {
	case SYSTEM_EVENT_STA_START:
		switch (esp_wifi_connect()) {
		case ESP_OK:
			ESP_LOGI(TAG, "connected successfully");
			xEventGroupSetBits(status_event_group, STATUS_EVENT_FIRMWARE);
			break;

		case ESP_ERR_WIFI_NOT_INIT:
			ESP_LOGE(TAG, "WiFi is not initialized by eps_wifi_init");
			break;

		case ESP_ERR_WIFI_NOT_STARTED:
			ESP_LOGE(TAG, "WiFi is not started by esp_wifi_start");
			break;

		case ESP_ERR_WIFI_CONN:
			ESP_LOGE(TAG,
					"WiFi internal error, station or soft-AP control block wrong");
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
	case SYSTEM_EVENT_STA_GOT_IP:
		xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        /* Start the web server */
        if (*server == NULL) {
            *server = start_webserver();
        }
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		/* This is a workaround as ESP32 WiFi libs don't currently
		 auto-reassociate. */
		esp_wifi_connect();
		xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        /* Stop the web server */
        if (*server) {
            stop_webserver(*server);
            *server = NULL;
        }
		break;
	case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
		/*point: the function esp_wifi_wps_start() only get ssid & password
		 * so call the function esp_wifi_connect() here
		 * */
		ESP_LOGI(TAG, "SYSTEM_EVENT_STA_WPS_ER_SUCCESS");
		ESP_ERROR_CHECK(esp_wifi_wps_disable());
		ESP_ERROR_CHECK(esp_wifi_connect());
		break;
	case SYSTEM_EVENT_STA_WPS_ER_FAILED:
		ESP_LOGI(TAG, "SYSTEM_EVENT_STA_WPS_ER_FAILED");
		ESP_ERROR_CHECK(esp_wifi_wps_disable());
		ESP_ERROR_CHECK(esp_wifi_wps_enable(&config));
		ESP_ERROR_CHECK(esp_wifi_wps_start(0));
		break;
	case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
		ESP_LOGI(TAG, "SYSTEM_EVENT_STA_WPS_ER_TIMEOUT");
		ESP_ERROR_CHECK(esp_wifi_wps_disable());
		ESP_ERROR_CHECK(esp_wifi_wps_enable(&config));
		ESP_ERROR_CHECK(esp_wifi_wps_start(0));
		break;
	case SYSTEM_EVENT_STA_WPS_ER_PIN:
		ESP_LOGI(TAG, "SYSTEM_EVENT_STA_WPS_ER_PIN");
		/*show the PIN code here*/
		ESP_LOGI(TAG, "WPS_PIN = "PINSTR,
				PIN2STR(event->event_info.sta_er_pin.pin_code))	;
		break;
	default:
		break;
	}
	return ESP_OK;
}

void wifi_init_sta(void *param)
{
    wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, param) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    while(1) {
    	if (!(xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT)) {
    		g_wifi_reconnect_flag = true;
			ESP_ERROR_CHECK(esp_wifi_init(&cfg));

			ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
			ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));

			ESP_ERROR_CHECK(esp_wifi_start() );

			vTaskDelay(5000/portTICK_PERIOD_MS);
    	}
    	if (!(xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT)) {

    		g_wifi_reconnect_flag = false;

			ESP_LOGI(TAG, "wifi_init_sta stop sta");
			ESP_ERROR_CHECK(esp_wifi_stop() );
			ESP_LOGI(TAG, "wifi_init_sta deinit sta");
			ESP_ERROR_CHECK(esp_wifi_deinit() );
    	}
		vTaskDelay(100/portTICK_PERIOD_MS);
    }
}

void app_main() {

    static httpd_handle_t server = NULL;
    button_event_group = xEventGroupCreate();
	mqtt_event_group = xEventGroupCreate();


	/* Initialize NVS — it is used to store PHY calibration data */
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	//initialise_wifi(&server);
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    xTaskCreate(wifi_init_sta, "wifi init task", 4096, &server, 10, NULL);

    status_task_setup();
	gpio_task_setup();
	setupSHT1xTask();
	mqtt_config_init();
	mqtt_user_init();
	mqtt_user_addHandler(&controlHandler);

	ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());

	time_t now, last;
	time(&last);
	time(&now);

	while (1) {
		//if (xEventGroupWaitBits(button_event_group, WPS_SHORT_BIT, true, true,10)) {
		if (enableWPS) {
			activateWPS(&config);
			enableWPS = false;
		}
		/*
		time(&now);
		if (difftime(now,last) >=2 ) {
			vTaskGetRunTimeStats(task_debug_buf);
			ESP_LOGI(TAG, "%s", task_debug_buf);
			last = now;
		}
		*/
		vTaskDelay(100/portTICK_PERIOD_MS);
	}
}
