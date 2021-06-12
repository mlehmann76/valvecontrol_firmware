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

#include "LedFlasher.h"
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

#include <stdlib.h>
#include <string.h>
#include <thread>

const char TAG[] = "WifiManager";

namespace wifi {

WifiManager::WifiManager(repository &_repo, std::mutex &m,
                         std::condition_variable &cv,
                         std::shared_ptr<LedFlasher> led)
    : mutex(m), cvInitDone(cv), m_mode(detail::NoMode()), m_repo(_repo),
      m_led(led) {
    auto cfg = esp_pthread_get_default_config();
    cfg.thread_name = "wifi task";
    esp_pthread_set_cfg(&cfg);
    m_thread = std::thread([this]() { this->task(); });
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

        // Init wifi
        ESP_ERROR_CHECK(esp_wifi_init(&m_config.wificfg));
        // set hostname
        esp_netif_t *pStaCfg = esp_netif_create_default_wifi_sta();
        esp_netif_t *pAPCfg = esp_netif_create_default_wifi_ap();
        ESP_ERROR_CHECK(
            esp_netif_set_hostname(pStaCfg, netConf.getHostname().c_str()));
        ESP_ERROR_CHECK(
            esp_netif_set_hostname(pAPCfg, netConf.getHostname().c_str()));
        // go to disconnect state
        detail::Events event =
            detail::TransitionEvent{this, detail::DisconnectState{this}};

        m_mode = std::visit(EventVisitor(m_mode), event);
        m_led->set(LedFlasher::OFF);
        m_initDone = true;
    }
    cvInitDone.notify_one();

    m_repo.create("/network/wifi/control/scan", {{{"start", false}}})
        .set([this](const property &) -> std::optional<property> {
            this->startScan();
            return {};
        });

    m_repo.create("/network/wifi/control/wps", {{{"start", false}}})
        .set([this](const property &) -> std::optional<property> {
            this->startWPS();
            return {};
        });
}

void WifiManager::task() {
    init();
    while (!m_atexit) {
        if (m_timeout.active && m_timeout.expired()) {
            m_events.push_back(m_timeout.event);
        }
        if (!m_events.empty()) {
            auto event = m_events.front();
            m_events.pop_front();
            m_mode = std::visit(EventVisitor(m_mode), event);
        }
        wifi_mode_t actmode;
        esp_err_t err = esp_wifi_get_mode(&actmode);
        wifi_mode_t repmode = static_cast<wifi_mode_t>(netConf.getMode());

        if (err == ESP_OK && actmode != repmode) {
            esp_wifi_set_mode(repmode);
            m_events.push_back(detail::ModeChangeEvent{});
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
    m_events.push_back(detail::WifiEvent{
        this, static_cast<wifi_event_t>(event_id), event_data});
}

void WifiManager::got_ip_event_handler(esp_event_base_t event_base,
                                       int32_t event_id, void *event_data) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    switch (event_id) {
    case IP_EVENT_STA_GOT_IP:
        log_inst.info(TAG,
                      "got ip:"
                      "%d.%d.%d.%d",
                      IP2STR(&event->ip_info.ip));
        // FIXME xEventGroupSetBits(main_event_group, CONNECTED_BIT);
        notifyConnect();
        m_timeout.stop();
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
    m_led->set(LedFlasher::ON);
}

void WifiManager::notifyDisconnect() {
    for (auto _c : m_observer) {
        _c->onDisconnect();
    }
    m_led->set(LedFlasher::OFF);
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
    // FIXME check wifi mode before transition
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

    m_parent->m_led->set(LedFlasher::OFF);

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

template <> WifiMode ConnectState::handle(const ModeChangeEvent &e) {
    m_parent->m_events.push_back(
        detail::TransitionEvent{m_parent, detail::DisconnectState{m_parent}});
    return *this;
}

void ConnectState::setAPConfig(wifi_config_t &wifi_config) {
    memset(&wifi_config, 0, sizeof(wifi_config));
    const size_t len = netConf.getApSSID().length();
    memcpy(wifi_config.ap.ssid, netConf.getApSSID().c_str(),
           len > 32 ? 32 : len);
    wifi_config.ap.ssid_len = len;
    wifi_config.ap.channel = netConf.getApChannel();
    /* prevent ap error if password shorter then 8 */
    if (netConf.getApPass().length() < 8) {
        netConf.setApPass("espressif"); // FIXME make configurable
    }
    const std::string _pass = netConf.getApPass();
    const size_t lenp = _pass.length();
    memcpy(wifi_config.ap.password, _pass.c_str(), lenp > 64 ? 64 : lenp);
    wifi_config.ap.max_connection = 1; // TODO
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
}

void ConnectState::setSTAConfig(const std::string &ssid,
                                wifi_config_t &wifi_config) {
    memset(&wifi_config, 0, sizeof(wifi_config));
    strcpy((char *)wifi_config.sta.ssid, ssid.c_str());
    strcpy((char *)wifi_config.sta.password, netConf.getStaPass().c_str());
}

void ConnectState::onEnter() {
    log_inst.info(TAG, "ConnectState start");
    wifi_mode_t mode = static_cast<wifi_mode_t>(netConf.getMode());
    wifi_config_t wifi_config;

    if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) {
        log_inst.debug(TAG, "ap ssid %s", netConf.getApSSID().c_str());
        setAPConfig(wifi_config);
        ESP_ERROR_CHECK(esp_wifi_set_mode(mode));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
    }

    if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA) {
        const std::string ssid = netConf.getStaSSID();
        if (ssid != "") {
            setSTAConfig(ssid, wifi_config);
            // log_inst.debug(TAG, "trying ssid {} {}", wifi_config.sta.ssid,
            // wifi_config.sta.password);
            log_inst.debug(TAG, "trying ssid %s", wifi_config.sta.ssid);

            ESP_ERROR_CHECK(esp_wifi_set_mode(mode));
            ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
        } else {
            ESP_ERROR_CHECK(esp_wifi_set_mode(mode));
            ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
        }
        /* TODO fallback, if two many tries ? */
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_ERROR_CHECK(esp_wifi_connect());
        m_parent->m_timeout.start(
            std::chrono::seconds(60),
            detail::TransitionEvent{m_parent,
                                    detail::DisconnectState{m_parent}});
    }

    // set default blinking
    m_parent->led()->set(LedFlasher::BLINK);
}

wifi_mode_t ConnectState::mode() const {
    wifi_mode_t mode;
    esp_err_t err = esp_wifi_get_mode(&mode);
    return err == ESP_OK ? mode : WIFI_MODE_NULL;
}

void ConnectState::onLeave() {
    wifi_sta_list_t sta_list;
    wifi_mode_t _m = mode();
    if ((_m == WIFI_MODE_AP || _m == WIFI_MODE_APSTA)) {
        if (ESP_OK == esp_wifi_ap_get_sta_list(&sta_list)) {
            if (sta_list.num == 0) {
                esp_wifi_disconnect();
            }
        }
    }
}

template <> WifiMode ConnectState::handle(const WifiEvent &e) {
    switch (e.m_id) {
    case WIFI_EVENT_WIFI_READY: /**< ESP32 WiFi ready */
        break;
    case WIFI_EVENT_STA_START: /**< ESP32 station start */
        esp_wifi_connect();
        break;
    case WIFI_EVENT_STA_STOP: /**< ESP32 station stop */
        break;
    case WIFI_EVENT_STA_CONNECTED: /**< ESP32 station connected to AP */
        // do not notify here, we have no ip ! m_parent->notifyConnect();
        break;
    case WIFI_EVENT_STA_DISCONNECTED: /**< ESP32 station disconnected from
                                       * AP
                                       */
        // stay in State if AP Mode
        if (mode() == WIFI_MODE_STA) {
            // return to disconnect state for further processing
            m_parent->m_events.push_back(detail::TransitionEvent{
                m_parent, detail::DisconnectState{m_parent}});
            // xEventGroupClearBits(main_event_group, CONNECTED_BIT);
            // m_parent->notifyDisconnect();
        }
        break;
    case WIFI_EVENT_STA_AUTHMODE_CHANGE: /**< the auth mode of AP connected
    by
                                            ESP32 station changed */
        break;
    case WIFI_EVENT_AP_START: /**< ESP32 soft-AP start */
        log_inst.debug(TAG, "soft-ap start");
        break;
    case WIFI_EVENT_AP_STOP: /**< ESP32 soft-AP stop */
        log_inst.debug(TAG, "soft-ap stop");
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
    m_parent->m_timeout.start(
        std::chrono::seconds(5),
        detail::TransitionEvent{m_parent, detail::DisconnectState{m_parent}});
}

template <> WifiMode ScanMode::handle(const WifiEvent &e) {
    switch (e.m_id) {
    case WIFI_EVENT_SCAN_DONE: /**< ESP32 finish scanning AP */
        log_inst.info(TAG, "WIFI_EVENT_SCAN_DONE");
        onWifiScanDone();
        // return to disconnect state for further processing
        m_parent->m_timeout.stop();
        m_parent->m_events.push_back(detail::TransitionEvent{
            m_parent, detail::DisconnectState(m_parent)});
        break;
    default:
        log_inst.info(TAG, "wifi event %d", int32_t(e.m_id));
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

    log_inst.debug(TAG, "scan found %d stations", ap_count);

    if (ap_count > 0) {
        wifi_ap_record_t *ap_info =
            (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * ap_count);

        if (ap_info) {
            memset(ap_info, 0, sizeof(wifi_ap_record_t) * ap_count);

            ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
            for (size_t i = 0; i < number; i++) {
                if (i < ap_count) {
                    m_parent->m_repo.create(
                        utilities::string_format(
                            "/network/wifi/state/scan/station%d", i),
                        {{
                            {"ssid", ssidToString(ap_info[i].ssid)},    //
                            {"auth", retauthMode(ap_info[i].authmode)}, //
                            {"rssi", IntType(ap_info[i].rssi)}          //
                        }});
                } else {
                    m_parent->m_repo.unlink(utilities::string_format(
                        "/network/wifi/state/scan/station%d", i));
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
    {
        log_inst.info(TAG, "SYSTEM_EVENT_STA_WPS_ER_SUCCESS");
        wifi_event_sta_wps_er_success_t *evt =
            (wifi_event_sta_wps_er_success_t *)e.event_data;
        /*
         * For only one AP credential don't sned event data, wps_finish() has
         * already set the config. This is for backward compatibility.
         */
        if (evt && evt->ap_cred_cnt) {
            /* If multiple AP credentials are received from WPS, connect with
             * first one */
            char *ssid = reinterpret_cast<char *>(evt->ap_cred[0].ssid);
            char *pass = reinterpret_cast<char *>(evt->ap_cred[0].passphrase);
            netConf.setStaSSID( //
                std::string(ssid, strnlen(ssid, MAX_SSID_LEN)));
            netConf.setStaPass( //
                std::string(pass, strnlen(pass, MAX_PASSPHRASE_LEN)));

            log_inst.debug(TAG, "Connecting to SSID: %s", ssid);
        } else {
            /*clear config, sta connect then will use system saved values*/
            netConf.setStaSSID("");
            netConf.setStaPass("");
        }
        m_parent->m_events.push_back(detail::TransitionEvent{
            this->m_parent, detail::ConnectState(this->m_parent)});
    } break;
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
    // start flashing led
    m_parent->led()->set(LedFlasher::BLINK, std::chrono::milliseconds(125),
                         std::chrono::milliseconds(375));

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
