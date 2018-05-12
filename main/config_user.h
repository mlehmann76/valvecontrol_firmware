/*
 * config_user.h
 *
 *  Created on: 11.05.2018
 *      Author: marco
 */

#ifndef MAIN_CONFIG_USER_H_
#define MAIN_CONFIG_USER_H_

#define CONTROL0_PIN	(GPIO_NUM_18)
#define CONTROL1_PIN	(GPIO_NUM_5)
#define CONTROL2_PIN	(GPIO_NUM_17)
#define CONTROL3_PIN	(GPIO_NUM_16)

#define CONTROL0_BIT	(GPIO_SEL_18)
#define CONTROL1_BIT	(GPIO_SEL_5)
#define CONTROL2_BIT	(GPIO_SEL_17)
#define CONTROL3_BIT	(GPIO_SEL_16)

#define AUTO_OFF_SEC	(35U*60U)
#endif /* MAIN_CONFIG_USER_H_ */
