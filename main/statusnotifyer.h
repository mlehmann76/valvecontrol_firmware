/*
 * .h
 *
 *  Created on: 06.09.2020
 *      Author: marco
 */

#ifndef MAIN_STATUSNOTIFYER_H_
#define MAIN_STATUSNOTIFYER_H_

#include "TimerCPP.h"

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
        : m_timeout("ConfigTimer", this, &StatusNotifyer::onTimeout,
                    (1000 / portTICK_PERIOD_MS), false),
          m_rep(rep), m_mqtt(&mqtt), m_topic(topic) {
        m_rep.create("/system/change/state/dateTime",
                     {{{"date", ""s}, {"time", ""s}}});
    }

    virtual ~StatusNotifyer() = default;

    void onSetNotify(const std::string &_name);

  private:
    void onTimeout();

    void updateDateTime();

    TimerMember<StatusNotifyer> m_timeout;
    repository &m_rep;
    mqtt::MqttWorker *m_mqtt;
    std::mutex m_lock;
    std::string m_topic;
};

inline void StatusNotifyer::onSetNotify(const std::string &_name) {
	std::unique_lock<std::mutex> lock1(m_lock, std::defer_lock);
    if (lock1.try_lock()) {
    	m_timeout.start();
	}
}

inline void StatusNotifyer::onTimeout() {
	std::unique_lock<std::mutex> lock1(m_lock, std::defer_lock);
	lock1.lock();
    updateDateTime();
    mqtt::MqttQueueType message(
        new mqtt::mqttMessage(m_topic, m_rep.stringify()));
    m_mqtt->send(std::move(message));
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
