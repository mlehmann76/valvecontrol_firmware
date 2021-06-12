/*
 * channelFactory.h
 *
 *  Created on: 14.07.2020
 *      Author: marco
 */

#ifndef MAIN_CHANNELFACTORY_H_
#define MAIN_CHANNELFACTORY_H_

#include "TimerCPP.h"
#include "channelBase.h"
#include "config_user.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

class ChannelFacory {
  public:
    virtual ~ChannelFacory() = default;
    virtual ChannelBase *channel(uint32_t index, std::chrono::seconds _p) = 0;
    virtual uint32_t count() const = 0;
};

class LedcChan : public ChannelBase {
    static constexpr std::chrono::milliseconds LED_C_TIME =
        std::chrono::milliseconds(250);
    enum mode { eoff, ehalf, efull };

  public:
    LedcChan(const char *_n, const ledc_channel_config_t _c,
             std::chrono::seconds _period);
    virtual ~LedcChan();
    virtual void set(bool, std::chrono::seconds);
    virtual bool get() const;

  private:
    void onTimer();
    void half();
    uint32_t get(mode m) const;

    ledc_channel_config_t _config;
    mode _mode;
    TimerMember<LedcChan> _timer;
    std::chrono::seconds _period;
};

class LedcChannelFactory {
    static const ledc_channel_config_t ledc_channel[];

  public:
    LedcChannelFactory();
    ~LedcChannelFactory() = default;
    static ChannelBase *channel(uint32_t index, std::chrono::seconds _p);
    static uint32_t count();
};
#endif /* MAIN_CHANNELFACTORY_H_ */
