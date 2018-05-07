/*
 * gpioTask.h
 *
 *  Created on: 06.05.2018
 *      Author: marco
 */

#ifndef MAIN_GPIOTASK_H_
#define MAIN_GPIOTASK_H_

extern QueueHandle_t subQueue,pubQueue;

typedef enum {mStatus, mOn, mOff} gpio_task_mode_t;

typedef struct {
	uint32_t chan;
	gpio_task_mode_t mode;
} queueData_t;


void gpio_task(void *pvParameters);


#endif /* MAIN_GPIOTASK_H_ */
