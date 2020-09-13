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
#include <functional>

class ChannelBase {
public:
	using notifyFuncType = std::function<void(ChannelBase *)>;
	ChannelBase(const char* _n) : m_name(_n) {};
	virtual ~ChannelBase() = default;
	virtual void set(bool, std::chrono::seconds) = 0;
	virtual bool get() const = 0;

	void notify() {
		for (auto m : m_notifier) {
			m(this);
		}
	}

	const char *name() const { return m_name.c_str(); }

	void add(notifyFuncType &&_a) {
		m_notifier.push_back(_a);
	}
protected:
	std::string m_name;
	std::vector<notifyFuncType> m_notifier;
};



#endif /* MAIN_CHANNELBASE_H_ */
