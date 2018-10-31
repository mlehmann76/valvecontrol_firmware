/*
 * config_user.h
 *
 *  Created on: 11.05.2018
 *      Author: marco
 */

#ifndef MAIN_CONFIG_USER_H_
#define MAIN_CONFIG_USER_H_

#ifdef __cplusplus
extern "C" {
#endif

#define LED_GPIO_PIN   (GPIO_NUM_27)
#define LED_GPIO_BIT   (GPIO_SEL_27)
#define LED_ON()		while(0) {gpio_set_level(LED_GPIO_PIN,0);}
#define LED_OFF()   	while(0) {gpio_set_level(LED_GPIO_PIN,1);}
#define LED_TOGGLE()   	while(0) {gpio_set_level(LED_GPIO_PIN,gpio_get_level(LED_GPIO_PIN)?0:1);}

#define WPS_BUTTON		(GPIO_SEL_0)

#define CONTROL0_PIN	(GPIO_NUM_18)
#define CONTROL1_PIN	(GPIO_NUM_5)
#define CONTROL2_PIN	(GPIO_NUM_17)
#define CONTROL3_PIN	(GPIO_NUM_16)

#define CONTROL0_BIT	(GPIO_SEL_18)
#define CONTROL1_BIT	(GPIO_SEL_5)
#define CONTROL2_BIT	(GPIO_SEL_17)
#define CONTROL3_BIT	(GPIO_SEL_16)

#define AUTO_OFF_SEC	(35U*60U)

#ifdef __cplusplus
}
#endif

#endif /* MAIN_CONFIG_USER_H_ */
