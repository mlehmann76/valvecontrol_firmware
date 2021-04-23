/*
 * LedFlasher.cpp
 *
 *  Created on: 01.04.2021
 *      Author: marco
 */

#include "LedFlasher.h"
#include "TimerCPP.h"
#include "config_user.h"
#include <stdint.h>

LedFlasher::LedFlasher(gpio_num_t gpio, bool isHighActive)
    : m_gpio(gpio), m_highActive(isHighActive), m_state(OFF),
      m_timer("ledflash", this, &LedFlasher::onTimer, 0, false) {
    gpio_config_t io_conf = {(1ULL << (uint64_t)gpio), GPIO_MODE_OUTPUT,
                             GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_DISABLE,
                             GPIO_INTR_DISABLE};
    gpio_config(&io_conf);
    gpio_set_level(m_gpio, m_highActive ? 0 : 1);
}

LedFlasher::~LedFlasher() { set(OFF); }

void LedFlasher::set(state state, std::chrono::milliseconds on,
                     std::chrono::milliseconds off) {
    m_state = state;
    m_on = on;
    m_off = off;
    m_timer.stop();
    switch (state) {
    case ON:
        gpio_set_level(m_gpio, m_highActive ? 1 : 0);
        break;
    case OFF:
        gpio_set_level(m_gpio, m_highActive ? 0 : 1);
        break;
    case BLINK:
        m_timer.period(std::chrono::duration_cast<portTick>(m_on).count());
        m_timer.start();
        gpio_set_level(m_gpio, m_highActive ? 1 : 0);
        break;
    }
}

void LedFlasher::onTimer() {
    if (m_state == BLINK) {
        if (gpio_get_level(m_gpio) == (m_highActive ? 1 : 0)) { // ON
            m_timer.period(std::chrono::duration_cast<portTick>(m_off).count());
            m_timer.start();
        } else {
            m_timer.period(std::chrono::duration_cast<portTick>(m_on).count());
            m_timer.start();
        }
    }
}
