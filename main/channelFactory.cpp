/*
 * channelFactory.cpp
 *
 *  Created on: 14.07.2020
 *      Author: marco
 */

#include <stdio.h>
#include <time.h>

#include "sdkconfig.h"
#include "config.h"
#include "config_user.h"

#include "TimerCPP.h"
#include "channelFactory.h"

const ledc_channel_config_t LedcChannelFactory::ledc_channel[] = { { //FIXME chanConfig.count()
		CONTROL0_PIN, LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_INTR_DISABLE, LEDC_TIMER_0, LED_C_OFF, 0, }, { //
		CONTROL1_PIN, LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, LEDC_INTR_DISABLE, LEDC_TIMER_0,	LED_C_OFF, 0, }, {
		CONTROL2_PIN, LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, LEDC_INTR_DISABLE, LEDC_TIMER_0,	LED_C_OFF, 0, }, {
		CONTROL3_PIN, LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_3, LEDC_INTR_DISABLE, LEDC_TIMER_0, LED_C_OFF, 0, } };

LedcChan::LedcChan(const char* _n, const ledc_channel_config_t _c, uint32_t _p) : AbstractChannel(_n),
		_config(_c), _mode(eoff), _timer("ledc", this, &LedcChan::onTimer, 0, false), _period(_p) {
	ledc_channel_config(&_c);
	set(false);
}

LedcChan::~LedcChan() {
	set(false);
}

void LedcChan::set(bool _b) {
	if (_b) {
		ledc_set_duty(_config.speed_mode, _config.channel, LED_C_ON);
		ledc_update_duty(_config.speed_mode, _config.channel);
		_timer.period(LED_C_TIME / portTICK_PERIOD_MS);
		_timer.start();
		_mode = efull;
	} else {
		ledc_set_duty(_config.speed_mode, _config.channel, LED_C_OFF);
		ledc_update_duty(_config.speed_mode, _config.channel);
		_timer.stop();
		_mode = eoff;
	}
	notify();
}

bool LedcChan::get() const {
	return (_mode == efull || _mode == ehalf);
}

void LedcChan::half() {
	ledc_set_duty(_config.speed_mode, _config.channel,LED_C_HALF);
	ledc_update_duty(_config.speed_mode, _config.channel);
	_timer.period(_period);
	_timer.start();
	_mode = ehalf;
}

void LedcChan::onTimer() {
	switch (_mode) {
	case efull:
		half();
		break;
	case ehalf:
		set(false);
		break;
	case eoff:
		break;
	}
}

LedcChannelFactory::LedcChannelFactory() {
	ledc_timer_config_t ledc_timer = { .speed_mode = LEDC_LOW_SPEED_MODE, .duty_resolution = LEDC_TIMER_13_BIT,
			.timer_num = LEDC_TIMER_0, .freq_hz = LED_FREQ, .clk_cfg = LEDC_AUTO_CLK };

	ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
}

AbstractChannel* LedcChannelFactory::channel(uint32_t index, uint32_t _periodInMs) {
	assert(index < count());
	return new LedcChan(chanConf.getName(index),ledc_channel[index], _periodInMs);
}

uint32_t LedcChannelFactory::count() const {
	return sizeof(ledc_channel)/sizeof(*ledc_channel);
}
