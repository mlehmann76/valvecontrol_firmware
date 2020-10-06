/*
 * channelFactory.cpp
 *
 *  Created on: 14.07.2020
 *      Author: marco
 */

#include <chrono>

#include "config_user.h"
#include "config.h"
#include "esp_log.h"

#include "TimerCPP.h"
#include "channelAdapter.h"
#include "channelFactory.h"

#define TAG "CHANNEL"

const ledc_channel_config_t LedcChannelFactory::ledc_channel[] = { { //FIXME chanConfig.count()
		CONTROL0_PIN, LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_INTR_DISABLE, LEDC_TIMER_0, LED_C_OFF, 0, }, { //
		CONTROL1_PIN, LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, LEDC_INTR_DISABLE, LEDC_TIMER_0, LED_C_OFF, 0, }, { //
		CONTROL2_PIN, LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, LEDC_INTR_DISABLE, LEDC_TIMER_0, LED_C_OFF, 0, }, { //
		CONTROL3_PIN, LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_3, LEDC_INTR_DISABLE, LEDC_TIMER_0, LED_C_OFF, 0, } }; //

LedcChan::LedcChan(const char *_n, const ledc_channel_config_t _c, std::chrono::seconds _p) :
		ChannelBase(_n), _config(_c), _mode(eoff), _timer("ledc", this, &LedcChan::onTimer,
				std::chrono::duration_cast<portTick>(_p).count(), false), _period(_p) {
	ledc_channel_config(&_c);
	set(false, _period);
}

LedcChan::~LedcChan() {
	set(false, _period);
}

void LedcChan::set(bool _b, std::chrono::seconds _time = std::chrono::seconds(-1)) {
	if (_b) {
		ledc_set_duty(_config.speed_mode, _config.channel, LED_C_ON);
		ledc_update_duty(_config.speed_mode, _config.channel);
		_timer.period(std::chrono::duration_cast<portTick>(LED_C_TIME).count());
		_timer.start();
		_mode = efull;
		_period = _time != std::chrono::seconds(-1) ? _time : _period;
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
	ledc_set_duty(_config.speed_mode, _config.channel, LED_C_HALF);
	ledc_update_duty(_config.speed_mode, _config.channel);
	_timer.period(std::chrono::duration_cast<portTick>(_period).count());
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

ChannelBase* LedcChannelFactory::channel(uint32_t index, std::chrono::seconds _p) {
	assert(index < count());
	return new LedcChan(chanConf.getName(index).c_str(), ledc_channel[index], _p);
}

uint32_t LedcChannelFactory::count() {
	return sizeof(ledc_channel) / sizeof(*ledc_channel);
}
