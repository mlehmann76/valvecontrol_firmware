/*
 * status.h
 *
 *  Created on: 25.03.2019
 *      Author: marco
 */

#ifndef MAIN_STATUS_H_
#define MAIN_STATUS_H_

#include "mqttUserTask.h"

class StatusProviderBase {
public:
	virtual ~StatusProviderBase() {}
	virtual bool hasUpdate() = 0;
	virtual void setUpdate(bool) = 0;
	virtual void addStatus(cJSON *) = 0;
};

template<class T>
class StatusProvider : public StatusProviderBase {
	friend T;
public:
	StatusProvider<T>(T *_c) : _client(_c) {}
	virtual bool hasUpdate() { return _client->hasUpdate(); }
	virtual void setUpdate(bool _b) { _client->setUpdate(_b);}
	virtual void addStatus(cJSON * _c) { _client->addStatus(_c);}
private:
	T *_client;
};


class StatusTask : public TaskClass {
public:
	StatusTask(EventGroupHandle_t &main, mqtt::PubQueue &_queue);
	virtual void task();
	void addProvider(StatusProviderBase &);
	virtual bool hasUpdate() {return m_update;}
	virtual void addStatus(cJSON *root) {
		addFirmwareStatus(root);
		addHardwareStatus(root);
	}
	void setUpdate(bool _up) { m_update = _up;}
	StatusProviderBase &status() { return m_status; }

private:
	void addTimeStamp(cJSON *root);
	void addFirmwareStatus(cJSON *root);
	void addHardwareStatus(cJSON *root);

	StatusProviderBase* m_statusFunc[8];
	size_t m_statusFuncCount;
	EventGroupHandle_t *main_event_group;
	mqtt::PubQueue &queue;
	StatusProvider<StatusTask> m_status;
	bool m_update = false;
};

#endif /* MAIN_STATUS_H_ */
