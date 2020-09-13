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


#endif /* MAIN_ABSTRACTCHANNELADAPTER_H_ */
