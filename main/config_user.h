/*
 * config_user.h
 *
 *  Created on: 11.05.2018
 *      Author: marco
 */

#ifndef MAIN_CONFIG_USER_H_
#define MAIN_CONFIG_USER_H_

#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"
#include <chrono>
#include <circular_policy.h>
#include <logger.h>

using logType =
    logger::Logger<logger::severity_type::debug, logger::ColoredTerminal,
                   logger::CircularLogPolicy<4>>;

extern logType log_inst;

#define LED_GREEN_GPIO_PIN (GPIO_NUM_26)
#define LED_YELLOW_GPIO_PIN (GPIO_NUM_27)

#define WPS_BUTTON (GPIO_SEL_0)
#define WPS_SHORT_MS 100
#define WPS_LONG_MS 500

//#define RESTART_BUTTON
#define RESTART_SHORT_MS 50
#define RESTART_LONG_MS 5000

#define NUM_CONTROL (4)
#define CONTROL0_PIN (CONFIG_CHANOUTPUT1)
#define CONTROL1_PIN (CONFIG_CHANOUTPUT2)
#define CONTROL2_PIN (CONFIG_CHANOUTPUT3)
#define CONTROL3_PIN (CONFIG_CHANOUTPUT4)

#define AUTO_OFF_SEC (35U * 60U)

#define CONNECTED_BIT (1u << 0)
#define MQTT_CONNECTED_BIT (1u << 1)
#define MQTT_OTA_BIT (1u << 2)
#define WPS_SHORT_BIT (1 << 3)
#define WPS_LONG_BIT (1 << 4)
#define SNTP_UPDATED (1u << 5)

using portTick =
    std::chrono::duration<TickType_t, std::ratio<1, CONFIG_FREERTOS_HZ>>;

#endif /* MAIN_CONFIG_USER_H_ */
