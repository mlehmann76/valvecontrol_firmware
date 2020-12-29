/*
 * WifiManager.cpp
 *
 *  Created on: 22.12.2020
 *      Author: marco
 */

#include "WifiManager.h"

#include "config.h"
#include "config_user.h"
#include "logger.h"

#include "QueueCPP.h"
#include "SemaphoreCPP.h"
#include "TimerCPP.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_wps.h"
#include "freertos/event_groups.h"
#include "repository.h"
#include <esp_netif.h>
#include <esp_pthread.h>
#include <fmt/printf.h>

#include <stdlib.h>
#include <string.h>
#include <thread>

const char TAG[] = "WifiManager";

namespace wifi {

WifiManager::WifiManager(repository &_repo, std::mutex &m,
                         std::condition_variable &cv)
    : mutex(m), cvInitDone(cv), m_mode(detail::NoMode()), m_repo(_repo) {
    auto cfg = esp_pthread_get_default_config();
    cfg.thread_name = "wifi task";
    esp_pthread_set_cfg(&cfg);
    m_thread = std::thread([this]() { this->task(); });
    _repo.create("/network/wifi/control/scan", {{{"start", false}}})
        .set([this](const property &) { this->startScan(); });
    _repo.create("/network/wifi/control/wps", {{{"start", false}}})
        .set([this](const property &) { this->startWPS(); });
}

struct EventVisitor {
    EventVisitor(detail::WifiMode &_p) : m_mode(_p) {}

    template <typename T> detail::WifiMode operator()(const T &event) const {
        return std::visit([event](auto &&mode) { return mode.handle(event); },
                          m_mode);
    }
    detail::WifiMode &m_mode;
};

void WifiManager::init() {
    {
        std::lock_guard<std::mutex> lck(mutex);
        esp_event_handler_instance_t instance_any_id;
        esp_event_handler_instance_t instance_got_ip;

        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID, &WifiManager::event_handler_s, this,
            &instance_any_id));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            IP_EVENT, IP_EVENT_STA_GOT_IP, &WifiManager::got_ip_event_handler_s,
            this, &instance_got_ip));

        // Init wifi stack and hostname
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_wifi_init(&m_config.wificfg));
        // set hostname
        esp_netif_t *pnetcfg = esp_netif_create_default_wifi_sta();
        esp_err_t ret =
            esp_netif_set_hostname(pnetcfg, netConf.getHostname().c_str());
        if (ret != ESP_OK) {
            log_inst.error(TAG, "failed to set hostname: {:d}", ret);
        } else {
            log_inst.debug(TAG, "hostname set to {:s}",
                           netConf.getHostname().c_str());
        }
        // go to disconnect state
        detail::Events event =
            detail::TransitionEvent{this, detail::DisconnectState{this}};

        m_mode = std::visit(EventVisitor(m_mode), event);
        m_initDone = true;
    }
    cvInitDone.notify_one();
}

void WifiManager::task() {
    init();
    while (1) {
        if (m_timeout.active && m_timeout.expired()) {
        }
        if (!m_events.empty()) {
            auto event = m_events.front();
            m_events.pop_front();
            m_mode = std::visit(EventVisitor(m_mode), event);
            // m_mode = std::visit(
            //     [this](auto &&event) {
            //         return std::visit(
            //             [event](auto &&mode) { return mode.handle(event); },
            //             this->m_mode);
            //     },
            //     event);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void WifiManager::event_handler_s(void *arg, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data) {
    WifiManager *_t = reinterpret_cast<WifiManager *>(arg);
    _t->event_handler(event_base, event_id, event_data);
}

void WifiManager::got_ip_event_handler_s(void *arg, esp_event_base_t event_base,
                                         int32_t event_id, void *event_data) {
    WifiManager *_t = reinterpret_cast<WifiManager *>(arg);
    _t->got_ip_event_handler(event_base, event_id, event_data);
}

void WifiManager::event_handler(esp_event_base_t event_base, int32_t event_id,
                                void *event_data) {
    m_events.push_back(
        detail::WifiEvent{this, static_cast<wifi_event_t>(event_id)});
}

void WifiManager::got_ip_event_handler(esp_event_base_t event_base,
                                       int32_t event_id, void *event_data) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    switch (event_id) {
    case IP_EVENT_STA_GOT_IP:
        log_inst.info(TAG,
                      "got ip:"
                      "{:d}.{:d}.{:d}.{:d}",
                      IP2STR(&event->ip_info.ip));
        // FIXME xEventGroupSetBits(main_event_group, CONNECTED_BIT);
        notifyConnect();
        break;
    case IP_EVENT_STA_LOST_IP:
        notifyDisconnect();
        break;
    }
}

void WifiManager::notifyConnect() {
    for (auto _c : m_observer) {
        _c->onConnect();
    }
}

void WifiManager::notifyDisconnect() {
    for (auto _c : m_observer) {
        _c->onDisconnect();
    }
}

void WifiManager::addConnectionObserver(iConnectionObserver &_obs) {
    m_observer.push_back(&_obs);
}

void WifiManager::remConnectionObserver(iConnectionObserver &_obs) {
    for (std::vector<iConnectionObserver *>::iterator it = m_observer.begin();
         it != m_observer.end(); ++it) {
        if ((*it) == &_obs) {
            delete (*it);
            m_observer.erase(it);
            break;
        }
    }
}

void WifiManager::startScan() {
    m_events.push_back(detail::TransitionEvent{this, detail::ScanMode{this}});
}

void WifiManager::startWPS() {
    m_wpsRetryCnt = m_config.wpsRetryCnt;
    m_events.push_back(detail::TransitionEvent{this, detail::WPSMode{this}});
}

namespace detail {

/**
 * NoMode
 **/

template <class T> WifiMode NoMode::handle(const T &) { return *this; }
template <> WifiMode NoMode::handle(const TransitionEvent &e) {
    return transition(*this, e.m_next);
}

/**
 * DisconnectState
 **/
template <class T> WifiMode DisconnectState::handle(const T &) { return *this; }
template <> WifiMode DisconnectState::handle(const TransitionEvent &e) {
    return transition(*this, e.m_next);
}

void DisconnectState::onEnter() {
    log_inst.debug(TAG, "DisconnectState enter");
    // prepare transition to new mode
    if (m_parent->m_config.initialScan) {
        m_parent->m_events.push_back(
            detail::TransitionEvent{m_parent, detail::ScanMode{m_parent}});
        m_parent->m_config.initialScan = false;
    } else {
        m_parent->m_events.push_back(
            detail::TransitionEvent{m_parent, detail::ConnectState{m_parent}});
    }
}

/**
 * ConnectState
 **/
template <class T> WifiMode ConnectState::handle(const T &) { return *this; }
template <> WifiMode ConnectState::handle(const TransitionEvent &e) {
    return transition(*this, e.m_next);
}

void ConnectState::onEnter() {
    log_inst.info(TAG, "ConnectState start");
    wifi_mode_t mode = static_cast<wifi_mode_t>(netConf.getMode());

    if (mode == WIFI_MODE_AP) {
        wifi_config_t wifi_config;
        memset(&wifi_config, 0, sizeof(wifi_config));

        const size_t len = netConf.getApSSID().length();
        memcpy(wifi_config.ap.ssid, netConf.getApSSID().c_str(),
               len > 32 ? 32 : len);
        wifi_config.ap.ssid_len = len;
        wifi_config.ap.channel = netConf.getApChannel();

        const size_t lenp = netConf.getApPass().length();
        memcpy(wifi_config.ap.password, netConf.getApPass().c_str(),
               lenp > 64 ? 64 : lenp);

        wifi_config.ap.max_connection = 1; // TODO
        wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
    } else {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_ERROR_CHECK(esp_wifi_connect());
        m_parent->m_timeout.start(
            std::chrono::seconds(10),
            detail::TransitionEvent{m_parent,
                                    detail::DisconnectState{m_parent}});
    }
}

void ConnectState::onLeave() { ESP_ERROR_CHECK(esp_wifi_disconnect()); }

template <> WifiMode ConnectState::handle(const WifiEvent &e) {
    switch (e.m_id) {
    case WIFI_EVENT_WIFI_READY: /**< ESP32 WiFi ready */
    case WIFI_EVENT_STA_START:  /**< ESP32 station start */
    case WIFI_EVENT_STA_STOP:   /**< ESP32 station stop */
        break;
    case WIFI_EVENT_STA_CONNECTED: /**< ESP32 station connected to AP */
        // do not notify here, we have no ip ! m_parent->notifyConnect();
        m_parent->m_timeout.stop();
        break;
    case WIFI_EVENT_STA_DISCONNECTED: /**< ESP32 station disconnected from
                                       * AP
                                       */
        // return to disconnect state for further processing
        // m_parent->m_events.push_back(detail::TransitionEvent{
        //     m_parent, detail::DisconnectState{m_parent}});
        // ESP_ERROR_CHECK(esp_wifi_connect());
        // xEventGroupClearBits(main_event_group, CONNECTED_BIT);
        // m_parent->notifyDisconnect();
        break;
    case WIFI_EVENT_STA_AUTHMODE_CHANGE: /**< the auth mode of AP connected
    by
                                            ESP32 station changed */
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
    default:
        break;
    }
    return (std::move(*this));
}

/**
 * ScanMode
 **/
template <class T> WifiMode ScanMode::handle(const T &) { return *this; }
template <> WifiMode ScanMode::handle(const TransitionEvent &e) {
    return transition(*this, e.m_next);
}

void ScanMode::onEnter() {
    wifi_scan_config_t scan_config;
    memset(&scan_config, 0, sizeof(scan_config));
    scan_config.scan_time.active.min = 10;
    scan_config.scan_time.active.max = 100;
    log_inst.debug(TAG, "wifi scan start");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, false));
}

template <> WifiMode ScanMode::handle(const WifiEvent &e) {
    switch (e.m_id) {
    case WIFI_EVENT_SCAN_DONE: /**< ESP32 finish scanning AP */
        log_inst.info(TAG, "WIFI_EVENT_SCAN_DONE");
        onWifiScanDone();
        // return to disconnect state for further processing
        m_parent->m_events.push_back(detail::TransitionEvent{
            m_parent, detail::DisconnectState(m_parent)});
        break;
    default:
        log_inst.info(TAG, "wifi event {:d}", int32_t(e.m_id));
        break;
    }
    return (std::move(*this));
}

const std::string ScanMode::retauthMode(wifi_auth_mode_t authmode) {
    switch (authmode) {
    case WIFI_AUTH_OPEN:
        return ("WIFI_AUTH_OPEN");
    case WIFI_AUTH_WEP:
        return ("WEP");
    case WIFI_AUTH_WPA_PSK:
        return ("WPA_PSK");
    case WIFI_AUTH_WPA2_PSK:
        return ("WPA2_PSK");
    case WIFI_AUTH_WPA_WPA2_PSK:
        return ("WPA_WPA2_PSK");
    case WIFI_AUTH_WPA2_ENTERPRISE:
        return ("WPA2_ENTERPRISE");
    case WIFI_AUTH_WPA3_PSK:
        return ("WPA3_PSK");
    case WIFI_AUTH_WPA2_WPA3_PSK:
        return ("WPA2_WPA3_PSK");
    default:
        return ("UNKNOWN");
    }
}

void ScanMode::onWifiScanDone() {

    uint16_t ap_count = 0;
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));

    log_inst.debug(TAG, "scan found {:d} stations", ap_count);

    if (ap_count > 0) {
        wifi_ap_record_t *ap_info =
            (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * ap_count);

        if (ap_info) {
            memset(ap_info, 0, sizeof(wifi_ap_record_t) * ap_count);

            ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
            for (size_t i = 0; i < number; i++) {
                if (i < ap_count) {
                    m_parent->m_repo.create(
                        fmt::format("/network/wifi/state/scan/station{:d}", i),
                        {{
                            {"ssid", ssidToString(ap_info[i].ssid)},    //
                            {"auth", retauthMode(ap_info[i].authmode)}, //
                            {"rssi", IntType(ap_info[i].rssi)}          //
                        }});
                } else {
                    m_parent->m_repo.unlink(
                        fmt::format("/network/wifi/state/scan/station{:d}", i));
                }
            }
            free(ap_info);
        } else {
            log_inst.error(TAG, "malloc failed");
        }
    }
}

/**
 * WPSMode
 **/
template <class T> WifiMode WPSMode::handle(const T &) { return *this; }
template <> WifiMode WPSMode::handle(const TransitionEvent &e) {
    return transition(*this, e.m_next);
}

template <> WifiMode WPSMode::handle(const WifiEvent &e) {
    switch (e.m_id) {
    case WIFI_EVENT_STA_WPS_ER_SUCCESS: /**< ESP32 station wps succeeds in
                                       enrollee mode */
        log_inst.info(TAG, "SYSTEM_EVENT_STA_WPS_ER_SUCCESS");
        /*
         * If only one AP credential is received from WPS, there will be no
         * event data and esp_wifi_set_config() is already called by WPS modules
         * for backward compatibility with legacy apps. So directly attempt
         * connection here.
         */
        // FIXME
        m_parent->m_events.push_back(detail::TransitionEvent{
            this->m_parent, detail::ConnectState(this->m_parent)});
        break;
    case WIFI_EVENT_STA_WPS_ER_FAILED: /**< ESP32 station wps fails in enrollee
                                          mode */
        log_inst.info(TAG, "SYSTEM_EVENT_STA_WPS_ER_FAILED");
        ESP_ERROR_CHECK(esp_wifi_wps_disable());
        ESP_ERROR_CHECK(esp_wifi_wps_enable(&m_parent->m_config.wps));
        ESP_ERROR_CHECK(esp_wifi_wps_start(0));
        break;
    case WIFI_EVENT_STA_WPS_ER_TIMEOUT: /**< ESP32 station wps timeout in
                                           enrollee mode */
        log_inst.info(TAG, "SYSTEM_EVENT_STA_WPS_ER_TIMEOUT");
        ESP_ERROR_CHECK(esp_wifi_wps_disable());
        ESP_ERROR_CHECK(esp_wifi_wps_enable(&m_parent->m_config.wps));
        ESP_ERROR_CHECK(esp_wifi_wps_start(0));
        break;
    case WIFI_EVENT_STA_WPS_ER_PIN: /**< ESP32 station wps pin code in enrollee
                                       mode */
        log_inst.info(TAG, "SYSTEM_EVENT_STA_WPS_ER_PIN");
        /*show the PIN code here*/
        // log_inst.info(
        //     TAG, "WPS_PIN = " PINSTR,
        //     PIN2STR(((wifi_event_sta_wps_er_pin_t *)event_data)->pin_code));
        break;
    case WIFI_EVENT_STA_WPS_ER_PBC_OVERLAP: /**< ESP32 station wps overlap in
                                               enrollee mode */

        break;
    default:
        break;
    }
    return (std::move(*this));
}

void WPSMode::onEnter() {
    if (m_parent->m_wpsRetryCnt > 0) {
        m_parent->m_wpsRetryCnt--;

        log_inst.info(TAG, "start wps...");
        ESP_ERROR_CHECK_WITHOUT_ABORT(
            esp_wifi_wps_enable(&m_parent->m_config.wps));
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_wps_start(0));
        m_parent->m_timeout.start(
            std::chrono::seconds(60),
            detail::TransitionEvent{m_parent, detail::WPSMode{m_parent}});
    } else {
        m_parent->m_timeout.start(
            std::chrono::seconds(60),
            detail::TransitionEvent{m_parent,
                                    detail::DisconnectState{m_parent}});
    }
}

void WPSMode::onLeave() {
    if (m_parent->m_wpsRetryCnt >= 0) {
        ESP_ERROR_CHECK(esp_wifi_wps_disable());
        ESP_ERROR_CHECK(esp_wifi_connect());
    }
}

} // namespace detail
} // namespace wifi