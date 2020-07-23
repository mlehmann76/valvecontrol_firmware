/*
 * channel.cpp
 *
 *  Created on: 23.07.2020
 *      Author: marco
 */

#include "channel.h"
#include "channelAdapter.h"

Channel::Channel(AbstractChannel *_chan) : m_chan(_chan){
}

Channel::~Channel() {
	// TODO Auto-generated destructor stub
}

void Channel::add(AbstractChannelAdapter* _a) {
	_a->setChannel(m_chan);
	m_adapter.push_back(_a);
}
