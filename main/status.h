/*
 * status.h
 *
 *  Created on: 25.03.2019
 *      Author: marco
 */

#ifndef MAIN_STATUS_H_
#define MAIN_STATUS_H_

#include "SemaphoreCPP.h"

class StatusProvider {
public:
	virtual ~StatusProvider() {}
	virtual bool hasUpdate() = 0;
	virtual void setUpdate(bool) = 0;
	virtual void addStatus(cJSON *) = 0;
};

class FirmwareStatus : public StatusProvider {
public:
	FirmwareStatus() : m_update(false), m_sem("status") {}
	virtual ~FirmwareStatus();
	virtual bool hasUpdate();
	virtual void addStatus(cJSON *);
	void setUpdate(bool _up) { if (m_sem.take(10)){ m_update = _up; m_sem.give();}}
private:
	bool m_update;
	Semaphore m_sem;
};

class HardwareStatus : public StatusProvider {
public:
	HardwareStatus() : m_update(false), m_sem("status") {}
	virtual ~HardwareStatus();
	virtual bool hasUpdate();
	virtual void addStatus(cJSON *);
	void setUpdate(bool _up) { if (m_sem.take(10)){ m_update = _up; m_sem.give();}}
private:
	bool m_update;
	Semaphore m_sem;
};

class StatusTask : public TaskClass {
public:
	StatusTask(EventGroupHandle_t &main);
	virtual void task();
	void addTimeStamp(cJSON *root);
	void addProvider(StatusProvider &);

private:
	StatusProvider* m_statusFunc[8];
	size_t m_statusFuncCount;
	EventGroupHandle_t *main_event_group;
};

#endif /* MAIN_STATUS_H_ */
