/*
 * channelFactory.cpp
 *
 *  Created on: 14.07.2020
 *      Author: marco
 */

#include <chrono>

#include "config.h"
#include "config_user.h"

#include "TimerCPP.h"
#include "channelAdapter.h"
#include "channelFactory.h"

#define TAG "CHANNEL"

#define LEDC_RESOLUTION LEDC_TIMER_13_BIT
#define LEDC_MAX ((1 << LEDC_RESOLUTION) - 1)
//#define LED_C_OFF (0)
//#define LED_C_ON (LEDC_MAX)
//#define LED_C_HALF ((LEDC_MAX * CONFIG_CHANOUT_PWM_DUTY) / 100)

const ledc_channel_config_t LedcChannelFactory::ledc_channel[] = {
    {
        // FIXME chanConfig.count()
        CONTROL0_PIN,
        LEDC_LOW_SPEED_MODE,
        LEDC_CHANNEL_0,
        LEDC_INTR_DISABLE,
        LEDC_TIMER_0,
        /*LED_C_OFF*/ 0,
        0,
    },
    {
        //
        CONTROL1_PIN,
        LEDC_LOW_SPEED_MODE,
        LEDC_CHANNEL_1,
        LEDC_INTR_DISABLE,
        LEDC_TIMER_0,
        /*LED_C_OFF*/ 0,
        0,
    },
    {
        //
        CONTROL2_PIN,
        LEDC_LOW_SPEED_MODE,
        LEDC_CHANNEL_2,
        LEDC_INTR_DISABLE,
        LEDC_TIMER_0,
        /*LED_C_OFF*/ 0,
        0,
    },
    {
        //
        CONTROL3_PIN,
        LEDC_LOW_SPEED_MODE,
        LEDC_CHANNEL_3,
        LEDC_INTR_DISABLE,
        LEDC_TIMER_0,
        /*LED_C_OFF*/ 0,
        0,
    }}; //

LedcChan::LedcChan(const char *_n, const ledc_channel_config_t _c,
                   std::chrono::seconds _p)
    : ChannelBase(_n), _config(_c), _mode(eoff),
      _timer("ledc", this, &LedcChan::onTimer,
             std::chrono::duration_cast<portTick>(_p).count(), false),
      _period(_p) {
    ledc_channel_config(&_c);
    set(false, _period);
}

LedcChan::~LedcChan() { /*FIXME set(false, _period, false);*/
}

void LedcChan::set(bool _b,
                   std::chrono::seconds _time = std::chrono::seconds(-1)) {
    if (_b) {
        ledc_set_duty(_config.speed_mode, _config.channel, get(efull));
        ledc_update_duty(_config.speed_mode, _config.channel);
        ESP_ERROR_CHECK_WITHOUT_ABORT(!_timer.period(
            std::chrono::duration_cast<portTick>(LED_C_TIME).count(), 1));
        ESP_ERROR_CHECK_WITHOUT_ABORT(!_timer.start(1));
        _mode = efull;
        _period = _time != std::chrono::seconds(-1) ? _time : _period;
    } else {
        ledc_set_duty(_config.speed_mode, _config.channel, get(eoff));
        ledc_update_duty(_config.speed_mode, _config.channel);
        ESP_ERROR_CHECK_WITHOUT_ABORT(!_timer.stop(1));
        _mode = eoff;
    }
    // log_inst.info(TAG, "channel %s, state %d, mode %d", m_name.c_str(), _b,
    // _mode);
    notify();
}

bool LedcChan::get() const { return (_mode == efull || _mode == ehalf); }

void LedcChan::half() {
    ledc_set_duty(_config.speed_mode, _config.channel, get(ehalf));
    ledc_update_duty(_config.speed_mode, _config.channel);
    ESP_ERROR_CHECK_WITHOUT_ABORT(!_timer.period(
        std::chrono::duration_cast<portTick>(_period).count(), 1));
    ESP_ERROR_CHECK_WITHOUT_ABORT(!_timer.start(1));
    _mode = ehalf;
}

void LedcChan::onTimer() {
    switch (_mode) {
    case efull:
        half();
        break;
    case ehalf:
    case eoff:
        set(false);
        break;
    }
}

uint32_t LedcChan::get(mode m) const {
    bool invert =
        Config::repo().get<int>("/actors/channels/config", "invert", false);
    float factor =
        Config::repo().get<double>("/actors/channels/config", "factor",
                                   (float)CONFIG_CHANOUT_PWM_DUTY / 100.0);
    uint32_t ret = 0;
    switch (m) {
    case eoff:
        ret = invert ? LEDC_MAX : 0;
        break;
    case ehalf:
        ret = invert ? LEDC_MAX * (1.0 - factor) : LEDC_MAX * (factor);
        break;
    case efull:
        ret = invert ? 0 : LEDC_MAX;
        break;
    }
    log_inst.debug(TAG, "channel %s, mode %d, invert %d, factor %f, value %d",
                  this->m_name.c_str(), _mode, invert, factor, (uint32_t)ret);
    return ret;
}

LedcChannelFactory::LedcChannelFactory() {
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = static_cast<uint32_t>(Config::repo().get<int>(
            "/actors/channels/config", "freq", CONFIG_CHANOUT_PWM_FREQ)),
        .clk_cfg = LEDC_AUTO_CLK};

    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
}

ChannelBase *LedcChannelFactory::channel(uint32_t index,
                                         std::chrono::seconds _p) {
    assert(index < count());
    return new LedcChan(chanConf.getName(index).c_str(), ledc_channel[index],
                        _p);
}

uint32_t LedcChannelFactory::count() {
    return sizeof(ledc_channel) / sizeof(*ledc_channel);
}
