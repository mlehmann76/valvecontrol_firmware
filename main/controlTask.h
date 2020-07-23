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
#include "QueueCPP.h"
#include "statusTask.h"

class AbstractChannel;
typedef Queue<AbstractChannel*, 8> GpioQueue;

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
	GpioQueue m_subQueue;
	EventGroupHandle_t *m_pMain;
	StatusProvider<ControlTask> m_status;
};

#endif /* MAIN_GPIOTASK_H_ */
