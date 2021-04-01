/*
 * WifiManager.h
 *
 *  Created on: 22.12.2020
 *      Author: marco
 */

#ifndef MAIN_WIFIMANAGER_H_
#define MAIN_WIFIMANAGER_H_

#include "iConnectionObserver.h"
#include "repository.h"
#include <chrono>
#include <condition_variable> // std::condition_variable
#include <deque>
#include <esp_wifi.h>
#include <esp_wps.h>
#include <mapbox/variant.hpp>
#include <mutex> // std::mutex, std::unique_lock
#include <vector>
#include <thread>

class LedFlasher;

namespace wifi {
class WifiManager;

namespace detail {

struct NoMode;
struct DisconnectState ;
struct ConnectState;
struct ScanMode;
struct WPSMode;
using WifiMode =
    mapbox::util::variant<NoMode, DisconnectState, ConnectState, ScanMode, WPSMode>;

struct NoMode {
    template <class T> WifiMode handle(const T &);
    void onEnter() {}
    void onLeave() {}
};

struct DisconnectState {
    DisconnectState(WifiManager *_p) : m_parent(_p) {}
    template <class T> WifiMode handle(const T &);
    void onEnter();
    void onLeave() {}
    WifiManager *m_parent;
};

struct ConnectState {
    ConnectState(WifiManager *_p) : m_parent(_p) {}
    template <class T> WifiMode handle(const T &);
    void onEnter();
    void onLeave();
    wifi_mode_t mode() const;
    WifiManager *m_parent;

  private:
    void setAPConfig(wifi_config_t &wifi_config);
    void setSTAConfig(const std::string &ssid, wifi_config_t &wifi_config);
};

struct ScanMode {
    static const size_t DEFAULT_SCAN_LIST_SIZE = 32;
    ScanMode(WifiManager *_p) : m_parent(_p) {}
    template <class T> WifiMode handle(const T &);
    void onEnter();
    void onLeave() {}
    static const std::string retauthMode(wifi_auth_mode_t authmode);
    std::string ssidToString(uint8_t *ssid);
    void onWifiScanDone();

    WifiManager *m_parent;
};

struct WPSMode {
    WPSMode(WifiManager *_p) : m_parent(_p) {}
    template <class T> WifiMode handle(const T &);
    void onEnter();
    void onLeave();
    WifiManager *m_parent;
};

template <typename T, typename N> struct transition {
    constexpr transition(T _mode, N _next) : m_mode(std::move(_mode)), m_next(std::move(_next)) {}
    operator WifiMode() {
        m_mode.onLeave();
        mapbox::util::apply_visitor([](auto &&arg) { arg.onEnter(); }, m_next);
        return std::move(m_next);
    }
    T m_mode;
    N m_next;
};
/**
 *
 **/
struct TimeOutEvent {};

struct TransitionEvent {
    TransitionEvent(WifiManager *_p, const WifiMode &_next)
        : m_parent(_p), m_next(std::move(_next)) {}
    WifiManager *m_parent;
    WifiMode m_next;
};

struct WifiEvent {
    WifiEvent(WifiManager *_p, wifi_event_t _id, void *_event_data) :
    	m_parent(_p), m_id(_id), event_data(_event_data) {}
    WifiManager *m_parent;
    wifi_event_t m_id;
    void *event_data;
};

using Events = mapbox::util::variant<TimeOutEvent, TransitionEvent, WifiEvent>;
/**
 *
 **/
class EventQueue {
    using value_type = detail::Events;
    using reference = detail::Events &;
    using const_reference = const detail::Events &;

  public:
    void push_back(const_reference &d) {
        std::lock_guard<std::mutex> lck(mutex);
        m_data.push_back(d);
    }
    reference front() {
        std::lock_guard<std::mutex> lck(mutex);
        return m_data.front();
    }
    void pop_front() {
        std::lock_guard<std::mutex> lck(mutex);
        m_data.pop_front();
    }
    bool empty() {
        std::lock_guard<std::mutex> lck(mutex);
        return m_data.empty();
    }

  private:
    std::deque<value_type> m_data;
    std::mutex mutex;
};

struct Timeout {
    void start(std::chrono::milliseconds t, const detail::Events &ev) {
        std::lock_guard<std::mutex> lck(mutex);
        _end = std::chrono::system_clock::now() + t;
        active = true;
        event = ev;
    }
    void stop() { active = false; }
    bool expired() {
        std::lock_guard<std::mutex> lck(mutex);
        bool ret = std::chrono::system_clock::now() > _end;
        active = ret ? false : true;
        return ret;
    }
    std::chrono::time_point<std::chrono::system_clock> _end;
    bool active = false;
    detail::Events event;
    std::mutex mutex;
};
} // namespace detail

class WifiManager {
    friend detail::DisconnectState;
    friend detail::ConnectState;
    friend detail::ScanMode;
    friend detail::WPSMode;

    struct config_t {
        bool initialScan = true;
        esp_wps_config_t wps = WPS_CONFIG_INIT_DEFAULT(WPS_TYPE_PBC);
        wifi_init_config_t wificfg = WIFI_INIT_CONFIG_DEFAULT();
        int wpsRetryCnt = 5;
    };

  public:
    WifiManager(repository &_repo, std::mutex &mutex,
                std::condition_variable &cvInitDone,
				std::shared_ptr<LedFlasher> led );
    virtual ~WifiManager() {
    	m_atexit=true;
    	if(m_thread.joinable()){
    		m_thread.join();
    	}
    }
    WifiManager &operator=(WifiManager &&other) = delete;
    WifiManager &operator=(const WifiManager &other) = delete;
    WifiManager(WifiManager &&other) = delete;
    WifiManager(const WifiManager &other) = delete;
    /**
     *
     */
    void task();
    void addConnectionObserver(iConnectionObserver &);
    void remConnectionObserver(iConnectionObserver &);
    /**
     */
    const detail::WifiMode &mode() const { return m_mode; }
    bool initDone() const { return m_initDone; }
    void startScan();
    void startWPS();

    LedFlasher* led() const {
    	return m_led.get();
    }

  private:
    static void event_handler_s(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data);
    static void got_ip_event_handler_s(void *arg, esp_event_base_t event_base,
                                       int32_t event_id, void *event_data);

    void event_handler(esp_event_base_t event_base, int32_t event_id,
                       void *event_data);
    void got_ip_event_handler(esp_event_base_t event_base, int32_t event_id,
                              void *event_data);
    void notifyConnect();
    void notifyDisconnect();
    void init();

    std::thread m_thread;
    std::mutex &mutex;
    std::condition_variable &cvInitDone;
    bool m_initDone = false;
    detail::WifiMode m_mode;
    repository &m_repo;
    std::vector<iConnectionObserver *> m_observer;
    detail::EventQueue m_events;
    config_t m_config;
    detail::Timeout m_timeout;
    int m_wpsRetryCnt;
    bool m_atexit = false;
    std::shared_ptr<LedFlasher> m_led;
};

} // namespace wifi

namespace wifi {
namespace detail {

inline std::string ScanMode::ssidToString(uint8_t *ssid) {
    return std::move(std::string(reinterpret_cast<char *>(ssid)));
}

} // namespace detail
} // namespace wifi
#endif /* MAIN_WIFIMANAGER_H_ */
