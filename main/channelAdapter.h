/*
 * ChannelAdapter.h
 *
 *  Created on: 23.07.2020
 *      Author: marco
 */

#ifndef MAIN_ABSTRACTCHANNELADAPTER_H_
#define MAIN_ABSTRACTCHANNELADAPTER_H_

#include "abstractchannel.h"
#include "mqtt_client.h"
#include "messager.h"

class AbstractChannelAdapter {
public:
	AbstractChannelAdapter() : m_channel(nullptr) {}
	virtual ~AbstractChannelAdapter() = default;
	AbstractChannel *channel() {return m_channel;}
	void setChannel(AbstractChannel *_channel) {m_channel = _channel;}
protected:
	AbstractChannel *m_channel;
};

/**
 *
 */
class DirectChannelAdapter : public AbstractChannelAdapter {
public:
	DirectChannelAdapter() : AbstractChannelAdapter() {}
};

/**
 *
 */

class MqttChannelAdapter : public AbstractChannelAdapter {
public:
	MqttChannelAdapter(Messager &_me, std::string topic) :
		AbstractChannelAdapter(), m_client(_me), m_topic(topic) {
		m_client.addHandle(this);
	}
	//
	int onMessage(esp_mqtt_event_handle_t event);
	//
private:
	Messager &m_client;
	std::string m_topic;
};

#endif /* MAIN_ABSTRACTCHANNELADAPTER_H_ */
