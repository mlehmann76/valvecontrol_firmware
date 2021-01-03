/*
 * .h
 *
 *  Created on: 06.09.2020
 *      Author: marco
 */

#ifndef MAIN_STATUSNOTIFYER_H_
#define MAIN_STATUSNOTIFYER_H_

#include "mqttWorker.h"
#include "repository.h"
#include <functional>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>

using namespace std::string_literals;

class StatusNotifyer {
  public:
    StatusNotifyer(repository &rep, mqtt::MqttWorker &mqtt, std::string topic)
        : m_rep(rep), m_mqtt(&mqtt), m_topic(topic), m_count(0) {
        m_rep.create("/system/change/state/dateTime",
                     {{{"date", ""s}, {"time", ""s}}});
        m_thread = std::thread([this] { task(); });
    }

    virtual ~StatusNotifyer() = default;

    void onSetNotify(const std::string &_name);

  private:
    void task();

    void updateDateTime();

    repository &m_rep;
    mqtt::MqttWorker *m_mqtt;
    std::mutex m_lock;
    std::string m_topic;
    std::thread m_thread;
    size_t m_count;
};

inline void StatusNotifyer::onSetNotify(const std::string &_name) {
    std::lock_guard<std::mutex> lock(m_lock);
    m_count++;
}

inline void StatusNotifyer::task() {
    while (true) {
        if (m_count) {
            updateDateTime();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::lock_guard<std::mutex> lock(m_lock);
            mqtt::MqttQueueType message(
                new mqtt::mqttMessage(m_topic, m_rep.stringify()));
            m_mqtt->send(std::move(message));
            m_count = 0;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
}

inline void StatusNotifyer::updateDateTime() {

    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    std::stringstream bdate, btime;
    bdate << std::put_time(&tm, "%F");
    m_rep.set("/system/change/state/dateTime", {{"date", bdate.str()}});
    btime << std::put_time(&tm, "%T");
    m_rep.set("/system/change/state/dateTime", {{"time", btime.str()}});
}

struct onStateNotify {
    onStateNotify(StatusNotifyer &rep) : m_rep(rep) {}
    void operator()(const std::string &s) { m_rep.onSetNotify(s); }
    StatusNotifyer &m_rep;
};

#endif /* MAIN_STATUSNOTIFYER_H_ */
