/*
 * gpioTask.h
 *
 *  Created on: 06.05.2018
 *      Author: marco
 */

#ifndef MAIN_GPIOTASK_H_
#define MAIN_GPIOTASK_H_

#ifdef __cplusplus
extern "C" {
#endif

#define WPS_SHORT_MS	(100)
#define WPS_LONG_MS (500)

extern QueueHandle_t pubQueue;
extern messageHandler_t controlHandler;

typedef enum {mStatus, mOn, mOff} gpio_task_mode_t;

typedef struct {
	uint32_t chan;
	gpio_task_mode_t mode;
} queueData_t;

void handleChannelControl(const cJSON* const chan);
int handleControlMsg(const char *, esp_mqtt_event_handle_t );
extern void gpio_task_setup(void);
void gpio_onConnect(void);
void addChannelStatus(cJSON *root);

#ifdef __cplusplus
}
#endif
#endif /* MAIN_GPIOTASK_H_ */
