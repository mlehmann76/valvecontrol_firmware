/*
 * WifiTask.cpp
 *
 *  Created on: 21.07.2020
 *      Author: marco
 */
#include <stdlib.h>
#include <string.h>

#include "config_user.h"
#include "sdkconfig.h"

#include "QueueCPP.h"
#include "SemaphoreCPP.h"
#include "TimerCPP.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_wps.h"
#include "freertos/event_groups.h"
#include <esp_pthread.h>

#if CONFIG_EXAMPLE_WPS_TYPE_PBC
#define WPS_TEST_MODE WPS_TYPE_PBC
#elif CONFIG_EXAMPLE_WPS_TYPE_PIN
#define WPS_TEST_MODE WPS_TYPE_PIN
#else
#define WPS_TEST_MODE WPS_TYPE_PBC
#endif /*CONFIG_EXAMPLE_WPS_TYPE_PBC*/

#include "WifiTask.h"

#ifndef PIN2STR
#define PIN2STR(a)                                                             \
    (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5], (a)[6], (a)[7]
#define PINSTR "%c%c%c%c%c%c%c%c"
#endif

#define WPS_SHORT_MS (100 / portTICK_PERIOD_MS)
#define WPS_LONG_MS (500 / portTICK_PERIOD_MS)

esp_wps_config_t WifiTask::config = WPS_CONFIG_INIT_DEFAULT(WPS_TEST_MODE);
const char WifiTask::TAG[] = "WifiTask";

WifiTask::WifiTask() {
    auto cfg = esp_pthread_get_default_config();
    cfg.thread_name = "wifi task";
    esp_pthread_set_cfg(&cfg);
    m_thread = std::thread([this]() { this->task(); });
}

WifiTask::~WifiTask() {}

void WifiTask::event_handler_s(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    WifiTask *_t = reinterpret_cast<WifiTask *>(arg);
    _t->event_handler(event_base, event_id, event_data);
}

void WifiTask::got_ip_event_handler_s(void *arg, esp_event_base_t event_base,
                                      int32_t event_id, void *event_data) {
    WifiTask *_t = reinterpret_cast<WifiTask *>(arg);
    _t->got_ip_event_handler(event_base, event_id, event_data);
}

void WifiTask::notifyConnect() {
    for (auto _c : m_observer) {
        _c->onConnect();
    }
}

void WifiTask::notifyDisconnect() {
    for (auto _c : m_observer) {
        _c->onDisconnect();
    }
}

void WifiTask::event_handler(esp_event_base_t event_base, int32_t event_id,
                             void *event_data) {
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
            ESP_LOGE(
                TAG,
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
    case WIFI_EVENT_STA_STOP: /**< ESP32 station stop */
        break;
    case WIFI_EVENT_STA_CONNECTED: /**< ESP32 station connected to AP */
        break;
    case WIFI_EVENT_STA_DISCONNECTED: /**< ESP32 station disconnected from AP */
        /* This is a workaround as ESP32 WiFi libs don't currently
         auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(main_event_group, CONNECTED_BIT);
        notifyDisconnect();
        break;
    case WIFI_EVENT_STA_AUTHMODE_CHANGE: /**< the auth mode of AP connected by
                                            ESP32 station changed */
        break;

    case WIFI_EVENT_STA_WPS_ER_SUCCESS: /**< ESP32 station wps succeeds in
                                           enrollee mode */
        /*point: the function esp_wifi_wps_start() only get ssid & password
         * so call the function esp_wifi_connect() here
         * */
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_WPS_ER_SUCCESS");
        ESP_ERROR_CHECK(esp_wifi_wps_disable());
        ESP_ERROR_CHECK(esp_wifi_connect());

        break;
    case WIFI_EVENT_STA_WPS_ER_FAILED: /**< ESP32 station wps fails in enrollee
                                          mode */
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_WPS_ER_FAILED");
        ESP_ERROR_CHECK(esp_wifi_wps_disable());
        ESP_ERROR_CHECK(esp_wifi_wps_enable(&config));
        ESP_ERROR_CHECK(esp_wifi_wps_start(0));
        break;
    case WIFI_EVENT_STA_WPS_ER_TIMEOUT: /**< ESP32 station wps timeout in
                                           enrollee mode */
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_WPS_ER_TIMEOUT");
        ESP_ERROR_CHECK(esp_wifi_wps_disable());
        ESP_ERROR_CHECK(esp_wifi_wps_enable(&config));
        ESP_ERROR_CHECK(esp_wifi_wps_start(0));
        break;
    case WIFI_EVENT_STA_WPS_ER_PIN: /**< ESP32 station wps pin code in enrollee
                                       mode */
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_WPS_ER_PIN");
        /*show the PIN code here*/
        ESP_LOGI(
            TAG, "WPS_PIN = " PINSTR,
            PIN2STR(((wifi_event_sta_wps_er_pin_t *)event_data)->pin_code));
        break;
    case WIFI_EVENT_STA_WPS_ER_PBC_OVERLAP: /**< ESP32 station wps overlap in
                                               enrollee mode */

        break;
    case WIFI_EVENT_AP_START: /**< ESP32 soft-AP start */
        break;
    case WIFI_EVENT_AP_STOP: /**< ESP32 soft-AP stop */
        break;
    case WIFI_EVENT_AP_STACONNECTED: /**< a station connected to ESP32 soft-AP
                                      */
        break;
    case WIFI_EVENT_AP_STADISCONNECTED: /**< a station disconnected from ESP32
                                           soft-AP */
        break;
    case WIFI_EVENT_AP_PROBEREQRECVED: /**< Receive probe request packet in
                                          soft-AP interface */
        break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
void WifiTask::got_ip_event_handler(esp_event_base_t event_base,
                                    int32_t event_id, void *event_data) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    switch (event_id) {
    case IP_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(main_event_group, CONNECTED_BIT);
        notifyConnect();
        break;
    case IP_EVENT_STA_LOST_IP:
        break;
    }
}

void WifiTask::task() {

    int timeout = 0;

    main_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &WifiTask::event_handler_s, this,
        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &WifiTask::got_ip_event_handler_s, this,
        &instance_got_ip));

    while (1) {
        checkWPSButton();

        EventBits_t uxBits;
        uxBits = xEventGroupClearBits(main_event_group, WPS_LONG_BIT);

        if ((uxBits & WPS_LONG_BIT) == WPS_LONG_BIT) {
            enableWPS = true;
        }

        switch (w_state) {
        case w_disconnected:
            if (!(ESP_OK == esp_wifi_init(&cfg))) {
                break;
            }
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
            ESP_ERROR_CHECK(esp_wifi_start());

            if (!enableWPS) {
                timeout = 300;
                w_state = w_connected;
            } else {
                timeout = 50;
                w_state = w_wps_enable;
            }
            break;

        case w_connected:
            if (!isConnected()) {
                if (timeout == 0) {
                    ESP_LOGI(TAG, "wifi_init_sta stop sta");
                    ESP_ERROR_CHECK(esp_wifi_stop());
                    ESP_LOGI(TAG, "wifi_init_sta deinit sta");
                    ESP_ERROR_CHECK(esp_wifi_deinit());
                    w_state = w_disconnected;
                } else {
                    timeout--;
                }
            } else {
                timeout = 300;
            }
            break;

        case w_wps_enable:
            if (!isConnected()) {
                if (timeout == 0) {
                    ESP_LOGI(TAG, "start wps...");
                    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_wps_enable(&config));
                    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_wps_start(0));
                    enableWPS = false;
                    timeout = 600;
                    w_state = w_wps;
                } else {
                    timeout--;
                }
            }
            break;

        case w_wps:
            if (isConnected()) {
                w_state = w_connected;
            } else {
                timeout--;
                if (timeout == 0) {
                    w_state = w_disconnected;
                }
            }
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool WifiTask::isConnected() {
    return (xEventGroupGetBits(main_event_group) & CONNECTED_BIT) != 0;
}

bool WifiTask::isMQTTConnected() {
    return (xEventGroupGetBits(main_event_group) & MQTT_CONNECTED_BIT) != 0;
}

int WifiTask::checkWPSButton() {
    static int wps_button_count = 0;
    if ((gpio_get_level((gpio_num_t)WPS_BUTTON) == 0)) {
        wps_button_count++;
        if (wps_button_count > (WPS_LONG_MS / portTICK_PERIOD_MS)) {
            xEventGroupSetBits(main_event_group, WPS_LONG_BIT);
            wps_button_count = 0;
        }
    } else {
        if (wps_button_count > (WPS_SHORT_MS / portTICK_PERIOD_MS)) {
            xEventGroupSetBits(main_event_group, WPS_SHORT_BIT);
        }
        wps_button_count = 0;
    }
    return wps_button_count;
}

void WifiTask::addConnectionObserver(iConnectionObserver &_obs) {
    m_observer.push_back(&_obs);
}

void WifiTask::remConnectionObserver(iConnectionObserver &_obs) {
    for (std::vector<iConnectionObserver *>::iterator it = m_observer.begin();
         it != m_observer.end(); ++it) {
        if ((*it) == &_obs) {
            delete (*it);
            m_observer.erase(it);
            break;
        }
    }
}
