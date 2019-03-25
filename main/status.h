/*
 * status.h
 *
 *  Created on: 25.03.2019
 *      Author: marco
 */

#ifndef MAIN_STATUS_H_
#define MAIN_STATUS_H_

EventGroupHandle_t status_event_group;

#define STATUS_EVENT_CONTROL BIT0

void status_task_setup(void);
void status_task(void *pvParameters);



#endif /* MAIN_STATUS_H_ */
