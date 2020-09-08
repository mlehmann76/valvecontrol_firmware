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
		repository(name, _tag) , m_mqtt(&mqtt), m_topic(topic) {
	}

	virtual ~StatusRepository() = default;

	/* set the property value */
	bool set(const std::string &name, const property &p) override {
		bool ret = repository::set(name, p);
		onRequest();
		return ret;
	}

private:

	void onRequest() {
		mqtt::MqttQueueType message(new mqtt::mqttMessage(m_topic, stringify()));
		m_mqtt->send(std::move(message));
	}

	mqtt::MqttWorker *m_mqtt;
	std::mutex m_lock;
	std::string m_topic;
};

#endif /* MAIN_STATUSREPOSITORY_H_ */
