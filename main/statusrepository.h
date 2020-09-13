/*
 * .h
 *
 *  Created on: 06.09.2020
 *      Author: marco
 */

#ifndef MAIN_STATUSREPOSITORY_H_
#define MAIN_STATUSREPOSITORY_H_

#include <string>
#include <memory>
#include <functional>
#include <mutex>
#include "mqttWorker.h"
#include "repository.h"

class StatusRepository : public repository {
public:
	template<class linkPolicy>
	StatusRepository(std::string name, mqtt::MqttWorker &mqtt, std::string topic, tag<linkPolicy> _tag) :
		repository(name, _tag) , m_mqtt(&mqtt), m_topic(topic), m_count(0){
		reg("dateTime", {{{"date",""},{"time",""}}});
		m_thread = std::thread([this]{task();});
	}

	virtual ~StatusRepository() = default;

	void onSetNotify(const std::string &_name) override {
		std::cout << name() << " : onSetNotify : "  << _name << std::endl;
		std::lock_guard<std::mutex> lock(m_lock);
		m_count++;
	}


private:

	void task() {
		while(true) {
			if (m_count) {
				updateDateTime();
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
				std::lock_guard<std::mutex> lock(m_lock);
				mqtt::MqttQueueType message(new mqtt::mqttMessage(m_topic, stringify()));
				m_mqtt->send(std::move(message));
				m_count = 0;
			} else {
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}
	}

	void updateDateTime();

	mqtt::MqttWorker *m_mqtt;
	std::mutex m_lock;
	std::string m_topic;
	std::thread m_thread;
	size_t m_count;
};

inline void StatusRepository::updateDateTime() {
	time_t now;
	struct tm timeinfo;
	time(&now);
	localtime_r(&now, &timeinfo);
	char strftime_buf[64];
	localtime_r(&now, &timeinfo);

	strftime(strftime_buf, sizeof(strftime_buf), "%F", &timeinfo);
	set("dateTime", {{"date", strftime_buf}});
	strftime(strftime_buf, sizeof(strftime_buf), "%T", &timeinfo);
	set("dateTime", {{"time", strftime_buf}});
}

#endif /* MAIN_STATUSREPOSITORY_H_ */
