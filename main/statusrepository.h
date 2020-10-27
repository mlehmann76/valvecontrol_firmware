/*
 * .h
 *
 *  Created on: 06.09.2020
 *      Author: marco
 */

#ifndef MAIN_STATUSREPOSITORY_H_
#define MAIN_STATUSREPOSITORY_H_

#include "mqttWorker.h"
#include "repository.h"
#include <functional>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>

using namespace std::string_literals;

class StatusRepository : public repository {
  public:
    template <class linkPolicy>
    StatusRepository(std::string name, mqtt::MqttWorker &mqtt,
                     std::string topic, tag<linkPolicy> _tag)
        : repository(name, _tag), m_mqtt(&mqtt), m_topic(topic), m_count(0) {
        create("dateTime", {{{"date", ""s}, {"time", ""s}}});
        m_thread = std::thread([this] { task(); });
    }

    virtual ~StatusRepository() = default;

    void onSetNotify(const std::string &_name) override;

  private:
    void task();

    void updateDateTime();

    mqtt::MqttWorker *m_mqtt;
    std::mutex m_lock;
    std::string m_topic;
    std::thread m_thread;
    size_t m_count;
};

inline void StatusRepository::onSetNotify(const std::string &_name) {
    std::lock_guard<std::mutex> lock(m_lock);
    m_count++;
}

inline void StatusRepository::task() {
    while (true) {
        if (m_count) {
            updateDateTime();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::lock_guard<std::mutex> lock(m_lock);
            mqtt::MqttQueueType message(
                new mqtt::mqttMessage(m_topic, stringify()));
            m_mqtt->send(std::move(message));
            m_count = 0;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
}

inline void StatusRepository::updateDateTime() {

    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    std::stringstream bdate, btime;
    bdate << std::put_time(&tm, "%F");
    set("dateTime", {{"date", bdate.str()}});
    btime << std::put_time(&tm, "%T");
    set("dateTime", {{"time", btime.str()}});
}

#endif /* MAIN_STATUSREPOSITORY_H_ */
