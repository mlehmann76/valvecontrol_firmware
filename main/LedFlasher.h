/*
 * LedFlasher.h
 *
 *  Created on: 01.04.2021
 *      Author: marco
 */

#ifndef MAIN_LEDFLASHER_H_
#define MAIN_LEDFLASHER_H_

#include <chrono>
#include "TimerCPP.h"
#include "driver/gpio.h"

class LedFlasher {
  public:
	enum state {OFF, ON, BLINK} ;
    LedFlasher(gpio_num_t gpio, bool isHighActive);
    virtual ~LedFlasher();
    LedFlasher(const LedFlasher &other) = delete;
    LedFlasher(LedFlasher &&other) = delete;
    LedFlasher &operator=(const LedFlasher &other) = delete;
    LedFlasher &operator=(LedFlasher &&other) = delete;

    void set(state,
    		std::chrono::milliseconds on = std::chrono::milliseconds(250),
			std::chrono::milliseconds off = std::chrono::milliseconds(250));

    state get() {
    	return m_state;
    }

  private:
    void onTimer();

    gpio_num_t m_gpio;
    bool m_highActive;
    state m_state;
    TimerMember<LedFlasher> m_timer;
    std::chrono::milliseconds m_on;
    std::chrono::milliseconds m_off;

};

#endif /* MAIN_LEDFLASHER_H_ */
