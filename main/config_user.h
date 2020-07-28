/*
 * config_user.h
 *
 *  Created on: 11.05.2018
 *      Author: marco
 */

#ifndef MAIN_CONFIG_USER_H_
#define MAIN_CONFIG_USER_H_

#include <chrono>
#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"

#define LED_GPIO_PIN   /*(GPIO_NUM_27)*/
#define LED_GPIO_BIT   /*(GPIO_SEL_27)*/
#define LED_ON()		/*while(0) {gpio_set_level(LED_GPIO_PIN,0);}*/
#define LED_OFF()   	/*while(0) {gpio_set_level(LED_GPIO_PIN,1);}*/
#define LED_TOGGLE()   	/*while(0) {gpio_set_level(LED_GPIO_PIN,gpio_get_level(LED_GPIO_PIN)?0:1);}*/

#define WPS_BUTTON		(GPIO_SEL_0)

#define NUM_CONTROL		(4)
#define CONTROL0_PIN	(CONFIG_CHANOUTPUT1)
#define CONTROL1_PIN	(CONFIG_CHANOUTPUT2)
#define CONTROL2_PIN	(CONFIG_CHANOUTPUT3)
#define CONTROL3_PIN	(CONFIG_CHANOUTPUT4)

#define AUTO_OFF_SEC	(35U*60U)

#define CONNECTED_BIT (1u<<0)
#define MQTT_CONNECTED_BIT (1u<<1)
#define MQTT_OTA_BIT (1u<<2)
#define WPS_SHORT_BIT	(1<<3)
#define WPS_LONG_BIT (1<<4)
#define SNTP_UPDATED (1u<<5)

using portTick = std::chrono::duration<TickType_t, std::ratio<1, CONFIG_FREERTOS_HZ>>;

#endif /* MAIN_CONFIG_USER_H_ */
