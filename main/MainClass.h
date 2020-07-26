/*
 * MainClass.h
 *
 *  Created on: 21.07.2020
 *      Author: marco
 */

#ifndef MAIN_MAINCLASS_H_
#define MAIN_MAINCLASS_H_

#include "TimerCPP.h"
#include "SemaphoreCPP.h"
#include "WifiTask.h"
#include "sht1x.h"
#include "mqttUserTask.h"
#include "mqtt_user_ota.h"
#include "sht1x.h"
#include "statusTask.h"
#include "messager.h"

class MainClass {
	WifiTask wifitask;
	Sht1x sht1x = {GPIO_NUM_21, GPIO_NUM_22};
	mqtt::MqttOtaWorker mqttOta;
	mqtt::MqttUserTask mqttUser;
	StatusTask status = {wifitask.eventGroup(), mqttUser.queue()};
	Messager messager;

public:
	int loop();
	static MainClass *instance() {
		static MainClass _inst;
		return &_inst;
	}
	Messager& getMessager() { return messager;}
	EventGroupHandle_t& eventGroup() {return (wifitask.eventGroup());}
	mqtt::MqttUserTask& mqtt() {return mqttUser;}
private:
	MainClass();
	virtual ~MainClass();
	void spiffsInit(void);
};

#endif /* MAIN_MAINCLASS_H_ */
