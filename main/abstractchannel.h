/*
 * channel.h
 *
 *  Created on: 23.07.2020
 *      Author: marco
 */

#ifndef MAIN_ABSTRACTCHANNEL_H_
#define MAIN_ABSTRACTCHANNEL_H_

#include <string>
#include <vector>

class AbstractChannelAdapter;

class AbstractChannel {
public:
	AbstractChannel(const char* _n) : _name(_n) {};
	virtual ~AbstractChannel() = default;
	virtual void set(bool,int) = 0;
	virtual bool get() const = 0;
	virtual void notify() = 0;
	const char *name() const { return _name.c_str(); }
	void add(AbstractChannelAdapter* _a) {m_adapter.push_back(_a);}
protected:
	std::string _name;
	std::vector<AbstractChannelAdapter*> m_adapter;
};



#endif /* MAIN_ABSTRACTCHANNEL_H_ */
