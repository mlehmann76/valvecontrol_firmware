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

typedef enum {
	mStatus, mOn, mOff
} gpio_task_mode_t;

typedef struct queueData {
	uint32_t chan;
	gpio_task_mode_t mode;
	uint32_t time;
	queueData() :
			chan(0), mode(mStatus), time(0) {
	}
	queueData(uint32_t _chan, gpio_task_mode_t _mode, uint32_t _time) :
			chan(_chan), mode(_mode), time(_time) {
	}
} queueData_t;

void handleChannelControl(const cJSON *const chan);
int handleControlMsg(const char*, esp_mqtt_event_handle_t);
extern void gpio_task_setup(void);

#ifdef __cplusplus
}
#endif

#include "status.h"

class ChannelStatus: public StatusProvider {
public:
	ChannelStatus(EventGroupHandle_t*);
	virtual ~ChannelStatus();
	virtual bool hasUpdate();
	virtual void addStatus(cJSON*);
public:
	void updateStatus(void);
	void checkMessage(queueData_t&);
	void checkTimeout();

private:
	enum channelMode_t {
		pOFF, pHALF, pON
	};

	struct channelSet_t {
		channelMode_t mode;
		time_t startTime;
		time_t runTime;
		channelSet_t() :
				mode(pOFF), startTime(0), runTime(0) {
		}
	};

	void updateChannel(uint32_t chan);
	void disableChan(uint32_t chan);
	void enableChan(uint32_t chan, time_t _runTime);

	static channelSet_t chanMode[]; //FIXME chanConfig.count()
	EventGroupHandle_t *m_status_event_group;

};

extern ChannelStatus channel;

#endif /* MAIN_GPIOTASK_H_ */
