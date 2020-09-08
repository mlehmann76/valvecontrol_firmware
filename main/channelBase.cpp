/*
 * channelBase.cpp
 *
 *  Created on: 06.09.2020
 *      Author: marco
 */

#include "channelAdapter.h"
#include "channelBase.h"

void ChannelBase::add(ChannelAdapterBase *_a) {
	_a->setChannel(this);
	m_adapter.push_back(_a);
}

