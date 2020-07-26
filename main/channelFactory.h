/*
 * channelFactory.h
 *
 *  Created on: 14.07.2020
 *      Author: marco
 */

#ifndef MAIN_ABSTRACTCHANNELFACTORY_H_
#define MAIN_ABSTRACTCHANNELFACTORY_H_

#include "abstractchannel.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

#define LEDC_RESOLUTION        LEDC_TIMER_13_BIT
#define LEDC_MAX			   ((1<<LEDC_RESOLUTION)-1)
#define LED_C_OFF			   LEDC_MAX


class ChannelFacory {
public:
	virtual ~ChannelFacory() = default;
	virtual AbstractChannel *channel(uint32_t index, uint32_t _periodInMs) = 0;
	virtual uint32_t count() const = 0;
};

class LedcChan: public AbstractChannel {
	static const uint32_t LED_C_ON = 0;
	static const uint32_t LED_C_HALF = ((LEDC_MAX * CONFIG_CHANOUT_PWM_DUTY) / 100);
	static const uint32_t LED_C_TIME = 250;
	enum mode {
		eoff, ehalf, efull
	};
public:
	LedcChan(const char* _n, const ledc_channel_config_t _c, uint32_t _periodInMs);
	virtual ~LedcChan();
	virtual void set(bool, int);
	virtual bool get() const;
	virtual void notify();
private:
	void onTimer();
	void half();

	ledc_channel_config_t _config;
	mode _mode;
	TimerMember<LedcChan> _timer;
	uint32_t _period;
};

class LedcChannelFactory {
	static const uint32_t LED_FREQ = (CONFIG_CHANOUT_PWM_FREQ);
	static const ledc_channel_config_t ledc_channel[];
public:
	LedcChannelFactory();
	~LedcChannelFactory() = default;
	static AbstractChannel *channel(uint32_t index, uint32_t _periodInMs);
	static uint32_t count();
};
#endif /* MAIN_ABSTRACTCHANNELFACTORY_H_ */
