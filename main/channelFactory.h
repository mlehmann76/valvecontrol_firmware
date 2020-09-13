/*
 * channelFactory.h
 *
 *  Created on: 14.07.2020
 *      Author: marco
 */

#ifndef MAIN_ABSTRACTCHANNELFACTORY_H_
#define MAIN_ABSTRACTCHANNELFACTORY_H_

#include "TimerCPP.h"
#include "config_user.h"
#include "channelBase.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

#define LEDC_RESOLUTION        LEDC_TIMER_13_BIT
#define LEDC_MAX			   ((1<<LEDC_RESOLUTION)-1)
#define LED_C_OFF			   LEDC_MAX


class ChannelFacory {
public:
	virtual ~ChannelFacory() = default;
	virtual ChannelBase *channel(uint32_t index, std::chrono::seconds _p) = 0;
	virtual uint32_t count() const = 0;
};

class LedcChan: public ChannelBase {
	static const uint32_t LED_C_ON = 0;
	static const uint32_t LED_C_HALF = ((LEDC_MAX * CONFIG_CHANOUT_PWM_DUTY) / 100);
	static constexpr std::chrono::milliseconds LED_C_TIME = std::chrono::milliseconds(250);
	enum mode {
		eoff, ehalf, efull
	};
public:
	LedcChan(const char* _n, const ledc_channel_config_t _c, std::chrono::seconds _period);
	virtual ~LedcChan();
	virtual void set(bool, std::chrono::seconds);
	virtual bool get() const;
private:
	void onTimer();
	void half();

	ledc_channel_config_t _config;
	mode _mode;
	TimerMember<LedcChan> _timer;
	std::chrono::seconds _period;
};

class LedcChannelFactory {
	static const uint32_t LED_FREQ = (CONFIG_CHANOUT_PWM_FREQ);
	static const ledc_channel_config_t ledc_channel[];
public:
	LedcChannelFactory();
	~LedcChannelFactory() = default;
	static ChannelBase *channel(uint32_t index, std::chrono::seconds _p);
	static uint32_t count();
};
#endif /* MAIN_ABSTRACTCHANNELFACTORY_H_ */
