/*
 * ChannelAdapter.h
 *
 *  Created on: 23.07.2020
 *      Author: marco
 */

#ifndef MAIN_ABSTRACTCHANNELADAPTER_H_
#define MAIN_ABSTRACTCHANNELADAPTER_H_

#include "channelBase.h"
#include "mqtt_client.h"
#include "messager.h"

class ChannelAdapterBase {
public:
	ChannelAdapterBase() : m_channel(nullptr) {}
	virtual ~ChannelAdapterBase() = default;
	virtual ChannelBase *channel() {return m_channel;}
	virtual void setChannel(ChannelBase *_channel) {m_channel = _channel;}
	virtual void onNotify(const ChannelBase*) = 0;
protected:
	ChannelBase *m_channel;
};

class MqttChannelAdapter;
class ChannelMqttreceiver : public AbstractMqttReceiver {
public:
	ChannelMqttreceiver(MqttChannelAdapter* _m) : m_adapter(_m) {}
	virtual int onMessage(esp_mqtt_event_handle_t event);
private:
	MqttChannelAdapter *m_adapter;
};
/**
 *
 */

class MqttChannelAdapter : public ChannelAdapterBase {
public:
	MqttChannelAdapter(Messager &_me, std::string subtopic, std::string pubtopic) :
		ChannelAdapterBase(), m_client(_me), m_rec(this), m_subtopic(subtopic), m_pubtopic(pubtopic) {
		m_client.addHandle(&m_rec);
	}
	//
	virtual int onMessage(esp_mqtt_event_handle_t event);
	virtual void onNotify(const ChannelBase*);
	//
protected:
	Messager &m_client;
	ChannelMqttreceiver m_rec;
	std::string m_subtopic;
	std::string m_pubtopic;
};

/**
 * control format for channel control
 {
 "channel": { "channel1": { "val": 1 } }
 }
 */
class MqttJsonChannelAdapter : public MqttChannelAdapter {
public:
	MqttJsonChannelAdapter(Messager &_me, std::string subtopic, std::string pubtopic) :
		MqttChannelAdapter(_me, subtopic, pubtopic) {}
	//
	virtual int onMessage(esp_mqtt_event_handle_t event);
	virtual void onNotify(const ChannelBase*);
	//
};

/*
 *
 */
class ExclusiveAdapter : public ChannelAdapterBase {
public:
	virtual ChannelBase *channel() ;
	virtual void setChannel(ChannelBase *_channel);
	virtual void onNotify(const ChannelBase*);
private:
	std::vector<ChannelBase*> m_vchannel;
};

inline int ChannelMqttreceiver::onMessage(esp_mqtt_event_handle_t event) {
	return m_adapter->onMessage(event);
}

#endif /* MAIN_ABSTRACTCHANNELADAPTER_H_ */
