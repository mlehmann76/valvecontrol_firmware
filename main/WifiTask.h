/*
 * WifiTask.h
 *
 *  Created on: 21.07.2020
 *      Author: marco
 */

#ifndef WIFITASK_H_
#define WIFITASK_H_

#include <vector>
#include <thread>
#include "freertos/event_groups.h"
#include "esp_wps.h"
#include "iConnectionObserver.h"

class WifiTask {
	static const char TAG[];
	static esp_wps_config_t config;
	enum state_t {
		w_disconnected, w_connected, w_wps_enable, w_wps
	} ;
public:
	WifiTask();
	virtual ~WifiTask();
	virtual void task();
	static WifiTask *instance() {
		static WifiTask _inst;
		return &_inst;
	}
	EventGroupHandle_t& eventGroup() {
		return (main_event_group);
	}
	bool isConnected();
	bool isMQTTConnected();
	void setEnableWps(bool _en = true) {enableWPS = _en;}
	void addConnectionObserver(iConnectionObserver&);
	void remConnectionObserver(iConnectionObserver&);

private:
	static void event_handler_s(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
	static void got_ip_event_handler_s(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

	void event_handler(esp_event_base_t event_base, int32_t event_id, void *event_data);
	void got_ip_event_handler(esp_event_base_t event_base, int32_t event_id, void *event_data);

	int checkWPSButton();
	void notifyConnect();
	void notifyDisconnect();

	std::thread m_thread;
	bool enableWPS = false;
	EventGroupHandle_t main_event_group = nullptr;
	state_t w_state = w_disconnected;
	std::vector<iConnectionObserver*> m_observer;
};


#endif /* WIFITASK_H_ */
