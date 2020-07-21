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

class Channel;
typedef Queue<Channel*, 8> GpioQueue;

class ControlTask: public TaskClass {
private:

public:
	ControlTask(EventGroupHandle_t &);
	virtual ~ControlTask();
	bool hasUpdate();
	void addStatus(cJSON*);
	void setUpdate(bool _up) { m_update = _up; }
	StatusProviderBase &status() { return m_status; }
	virtual void task();
	void setup();

private:
	const GpioQueue& getSubQueue() const {
		return m_subQueue;
	}
private:
	bool m_isConnected;
	bool m_update;
	Semaphore m_sem;
	GpioQueue m_subQueue;
	EventGroupHandle_t *m_pMain;
	StatusProvider<ControlTask> m_status;
};

#endif /* MAIN_GPIOTASK_H_ */
