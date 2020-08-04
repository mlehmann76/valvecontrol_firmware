/*
 * MainClass.h
 *
 *  Created on: 21.07.2020
 *      Author: marco
 */

#ifndef MAIN_MAINCLASS_H_
#define MAIN_MAINCLASS_H_

#include "mqttWorker.h"
#include "TimerCPP.h"
#include "SemaphoreCPP.h"
#include "WifiTask.h"
#include "sht1x.h"
#include "otaWorker.h"
#include "sht1x.h"
#include "statusTask.h"

class MainClass {
	WifiTask wifitask;
	//Sht1x sht1x = {GPIO_NUM_21, GPIO_NUM_22};
	Ota::OtaWorker otaWorker;
	mqtt::MqttWorker mqttUser;
	StatusTask status = {wifitask.eventGroup()};

public:
	int loop();
	static MainClass *instance() {
		static MainClass _inst;
		return &_inst;
	}
	EventGroupHandle_t& eventGroup() {return (wifitask.eventGroup());}
	mqtt::MqttWorker& mqtt() {return mqttUser;}
private:
	MainClass() = default;
	virtual ~MainClass() = default;
	void spiffsInit(void);
};

#endif /* MAIN_MAINCLASS_H_ */
