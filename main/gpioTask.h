/*
 * gpioTask.h
 *
 *  Created on: 06.05.2018
 *      Author: marco
 */

#ifndef MAIN_GPIOTASK_H_
#define MAIN_GPIOTASK_H_


#define WPS_SHORT_BIT	(1<<0)
#define WPS_LONG_BIT (1<<1)
#define WPS_SHORT_MS	(100)
#define WPS_LONG_MS (500)

QueueHandle_t subQueue,pubQueue;
EventGroupHandle_t button_event_group;

typedef enum {mStatus, mOn, mOff} gpio_task_mode_t;

typedef struct {
	uint32_t chan;
	gpio_task_mode_t mode;
} queueData_t;


void gpio_task(void *pvParameters);
void gpio_task_setup(void);


#endif /* MAIN_GPIOTASK_H_ */
