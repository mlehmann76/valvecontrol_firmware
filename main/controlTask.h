/*
 * gpioTask.h
 *
 *  Created on: 06.05.2018
 *      Author: marco
 */

#ifndef MAIN_GPIOTASK_H_
#define MAIN_GPIOTASK_H_

#define WPS_SHORT_MS	(100)
#define WPS_LONG_MS (500)

QueueHandle_t subQueue,pubQueue;
messageHandler_t controlHandler;

typedef enum {mStatus, mOn, mOff} gpio_task_mode_t;

typedef struct {
	uint32_t chan;
	gpio_task_mode_t mode;
} queueData_t;

void handleChannelControl(const cJSON* const chan);
int handleControlMsg(const char *, esp_mqtt_event_handle_t );
void gpio_task_setup(void);
void gpio_onConnect(void);
void addChannelStatus(cJSON *root);


#endif /* MAIN_GPIOTASK_H_ */
