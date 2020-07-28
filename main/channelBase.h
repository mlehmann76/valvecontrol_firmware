/*
 * channel.h
 *
 *  Created on: 23.07.2020
 *      Author: marco
 */

#ifndef MAIN_CHANNELBASE_H_
#define MAIN_CHANNELBASE_H_

#include <string>
#include <vector>
#include <chrono>

class ChannelAdapterBase;

class ChannelBase {
public:
	ChannelBase(const char* _n) : m_name(_n) {};
	virtual ~ChannelBase() = default;
	virtual void set(bool, std::chrono::seconds) = 0;
	virtual bool get() const = 0;
	virtual void notify() = 0;
	const char *name() const { return m_name.c_str(); }
	void add(ChannelAdapterBase *_a);
protected:
	std::string m_name;
	std::vector<ChannelAdapterBase*> m_adapter;
};



#endif /* MAIN_CHANNELBASE_H_ */
