/*
 * channelFactory.h
 *
 *  Created on: 14.07.2020
 *      Author: marco
 */

#ifndef MAIN_CHANNELFACTORY_H_
#define MAIN_CHANNELFACTORY_H_

#include "driver/gpio.h"
#include "driver/ledc.h"
#include <string>

#define LEDC_RESOLUTION        LEDC_TIMER_13_BIT
#define LEDC_MAX			   ((1<<LEDC_RESOLUTION)-1)
#define LED_C_OFF			   LEDC_MAX

class Channel {
public:
	Channel(const char* _n) : _name(_n) {};
	virtual ~Channel() = default;
	virtual void on() = 0;
	virtual void off() = 0;
	virtual bool active() = 0;
	const char *name() const { return _name.c_str(); }
protected:
	std::string _name;
};

class ChannelFacory {
public:
	virtual ~ChannelFacory() = default;
	virtual Channel *channel(uint32_t index, uint32_t _periodInMs) = 0;
	virtual uint32_t count() const = 0;
};

class LedcChan: public Channel {
	static const uint32_t LED_C_ON = 0;
	static const uint32_t LED_C_HALF = ((LEDC_MAX * CONFIG_CHANOUT_PWM_DUTY) / 100);
	static const uint32_t LED_C_TIME = 250;
	enum mode {
		eoff, ehalf, efull
	};
public:
	LedcChan(const char* _n, const ledc_channel_config_t _c, uint32_t _periodInMs);
	virtual ~LedcChan();
	void on();
	void off();
	bool active();
private:
	void onTimer();
	void half();

	ledc_channel_config_t _config;
	mode _mode;
	TimerMember<LedcChan> _timer;
	uint32_t _period;
};

class LedcChannelFactory : public ChannelFacory {
	static const uint32_t LED_FREQ = (CONFIG_CHANOUT_PWM_FREQ);
	static const ledc_channel_config_t ledc_channel[];
public:
	LedcChannelFactory();
	~LedcChannelFactory() = default;
	virtual Channel *channel(uint32_t index, uint32_t _periodInMs);
	virtual uint32_t count() const;
};
#endif /* MAIN_CHANNELFACTORY_H_ */
