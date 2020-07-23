/*
 * channel.h
 *
 *  Created on: 23.07.2020
 *      Author: marco
 */

#ifndef MAIN_CHANNEL_H_
#define MAIN_CHANNEL_H_

#include <vector>

class AbstractChannel;
class AbstractChannelAdapter;

class Channel {
public:
	Channel(AbstractChannel *_chan);
	virtual ~Channel();
	void add(AbstractChannelAdapter*);
private:
	AbstractChannel *m_chan;
	std::vector<AbstractChannelAdapter*> m_adapter;
};

#endif /* MAIN_CHANNEL_H_ */
