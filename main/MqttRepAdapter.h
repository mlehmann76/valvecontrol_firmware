/*
 * MqttRepAdapter.h
 *
 *  Created on: 05.09.2020
 *      Author: marco
 */

#ifndef MAIN_MQTTREPADAPTER_H_
#define MAIN_MQTTREPADAPTER_H_

#include <string>
#include "mqttWorker.h"
#include "repository.h"

class MqttRepAdapter: public mqtt::AbstractMqttReceiver {
public:
	MqttRepAdapter(repository &_r, mqtt::MqttWorker &_m, const std::string &_t);
	virtual ~MqttRepAdapter() = default;
	MqttRepAdapter(const MqttRepAdapter &other) = default;
	MqttRepAdapter(MqttRepAdapter &&other) = default;
	MqttRepAdapter& operator=(const MqttRepAdapter &other) = default;
	MqttRepAdapter& operator=(MqttRepAdapter &&other) = default;
	//
	virtual int onMessage(esp_mqtt_event_handle_t event);
	virtual std::string topic() { return m_topic; }
private:
	repository *m_repo;
	mqtt::MqttWorker *m_mqtt;
	std::string m_topic;
};

#endif /* MAIN_MQTTREPADAPTER_H_ */
