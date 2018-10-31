/*
 * config_user.c
 *
 *  Created on: 31.10.2018
 *      Author: marco
 */

#include "driver/gpio.h"
#include "config_user.h"

void board_init() {
	gpio_config_t config = {
			.pin_bit_mask = (LED_GPIO_OUTPUT),
			.mode = GPIO_MODE_OUTPUT};
	gpio_config(&config);
	LED_OFF();
}



