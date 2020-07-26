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
	virtual void onNotify() = 0;
protected:
	AbstractChannel *m_channel;
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
	virtual int onMessage(esp_mqtt_event_handle_t event);
	virtual void onNotify();
	//
protected:
	Messager &m_client;
	std::string m_topic;
};

/**
 * control format for channel control
 {
 "channel": { "channel1": { "val": 1 } }
 }
 */
class MqttJsonChannelAdapter : public MqttChannelAdapter {
public:
	MqttJsonChannelAdapter(Messager &_me, std::string topic) :
		MqttChannelAdapter(_me, topic) {}
	//
	virtual int onMessage(esp_mqtt_event_handle_t event);
	virtual void onNotify();
	//
};

#endif /* MAIN_ABSTRACTCHANNELADAPTER_H_ */
