/*
 * gpioTask.h
 *
 *  Created on: 06.05.2018
 *      Author: marco
 */

#ifndef MAIN_GPIOTASK_H_
#define MAIN_GPIOTASK_H_

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "status.h"
#include "QueueCPP.h"

#define WPS_SHORT_MS	(100)
#define WPS_LONG_MS (500)

typedef enum {
	mStatus, mOn, mOff
} gpio_task_mode_t;

struct queueData {
	uint32_t chan;
	gpio_task_mode_t mode;
	uint32_t time;
	queueData(uint32_t _chan = -1, gpio_task_mode_t _mode = mStatus, uint32_t _time = 0) :
			chan(_chan), mode(_mode), time(_time) {
	}
};

typedef Queue<queueData, 0> GpioQueue;

class GpioTask: public StatusProvider, public TaskClass {
private:
	enum channelMode_t {pOFF, pHALF, pON};

	struct channelSet_t {
		channelMode_t mode;
		time_t startTime;
		time_t runTime;
		channelSet_t() :
				mode(pOFF), startTime(0), runTime(0) {
		}
	};

public:
	GpioTask(EventGroupHandle_t &);
	virtual ~GpioTask();
	virtual bool hasUpdate();
	virtual void addStatus(cJSON*);
	void setUpdate(bool _up) { m_sem.take(); m_update = _up; m_sem.give();}
	virtual void task();
	void setup();

	int handleControlMsg(const char * topic, esp_mqtt_event_handle_t event);
	void handleChannelControl(const cJSON* const chan);
private:
	void checkMessage(queueData&);
	void checkTimeout();

	void updateChannel(uint32_t chan);
	void disableChan(uint32_t chan);
	void enableChan(uint32_t chan, time_t _runTime);
	int checkWPSButton();
	const GpioQueue& getSubQueue() const {
		return m_subQueue;
	}
private:
	static channelSet_t chanMode[]; //FIXME chanConfig.count()
	static ledc_channel_config_t ledc_channel[];
	bool m_isConnected;
	bool m_update;
	Semaphore m_sem;
	GpioQueue m_subQueue;
	EventGroupHandle_t *m_pMain;
};

#endif /* MAIN_GPIOTASK_H_ */
