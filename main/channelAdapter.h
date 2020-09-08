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
#include "mqttWorker.h"
#include "repository.h"

class ChannelAdapterBase {
public:
	virtual ~ChannelAdapterBase() = default;
	virtual ChannelBase *channel() {return m_channel;}
	virtual void setChannel(ChannelBase *_channel) {m_channel = _channel;}
	virtual void onNotify(const ChannelBase*) = 0;
protected:
	ChannelBase *m_channel;
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

/**
 *
 */
class RepositoryNotifier : public ChannelAdapterBase {
public:
	RepositoryNotifier(repository &_r, std::string name) : m_repos(_r), m_name(name) {
		_r.reg(name, {{"value", "OFF"}});
	}
	virtual void onNotify(const ChannelBase*) {
		m_repos.set(m_name, {{"value", channel()->get() ? "ON" : "OFF"}});
	}
private:
	repository &m_repos;
	std::string m_name;
};

#endif /* MAIN_ABSTRACTCHANNELADAPTER_H_ */
