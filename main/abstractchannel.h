/*
 * channel.h
 *
 *  Created on: 23.07.2020
 *      Author: marco
 */

#ifndef MAIN_ABSTRACTCHANNEL_H_
#define MAIN_ABSTRACTCHANNEL_H_

#include <string>

class AbstractChannel {
public:
	AbstractChannel(const char* _n) : _name(_n) {};
	virtual ~AbstractChannel() = default;
	virtual void set(bool) = 0;
	virtual bool get() const = 0;
	virtual void notify() = 0;
	const char *name() const { return _name.c_str(); }
private:
	std::string _name;
};



#endif /* MAIN_ABSTRACTCHANNEL_H_ */
