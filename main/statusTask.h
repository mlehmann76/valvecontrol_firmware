/*
 * status.h
 *
 *  Created on: 25.03.2019
 *      Author: marco
 */

#ifndef MAIN_STATUSTASK_H_
#define MAIN_STATUSTASK_H_

#include <vector>
#include <thread>
#include "mqttWorker.h"
#include "json.hpp"

class StatusProviderBase {
public:
	virtual ~StatusProviderBase() {}
	virtual bool hasUpdate() = 0;
	virtual void setUpdate(bool) = 0;
	virtual void addStatus(nlohmann::json *) = 0;
};

template<class T>
class StatusProvider : public StatusProviderBase {
	friend T;
public:
	StatusProvider<T>(T *_c) : _client(_c) {}
	bool hasUpdate() { return _client->hasUpdate(); }
	void setUpdate(bool _b) { _client->setUpdate(_b);}
	void addStatus(nlohmann::json * _c) { _client->addStatus(_c);}
private:
	T *_client;
};


class StatusTask {
public:
	StatusTask(EventGroupHandle_t &main);
	void task();
	void addProvider(StatusProviderBase &);
	bool hasUpdate() {return m_update;}
	void addStatus(nlohmann::json *root) {
		addFirmwareStatus(root);
		addHardwareStatus(root);
	}
	void setUpdate(bool _up) { m_update = _up;}
	StatusProviderBase &status() { return m_status; }

	static void addTimeStamp(nlohmann::json *root);
	static void addFirmwareStatus(nlohmann::json *root);
	static void addHardwareStatus(nlohmann::json *root);

private:
	std::thread m_thread;
	std::vector<StatusProviderBase*> m_statusFunc;
	EventGroupHandle_t *main_event_group;
	StatusProvider<StatusTask> m_status;
	bool m_update = false;
	bool m_isConnected = false;
};

#endif /* MAIN_STATUSTASK_H_ */
