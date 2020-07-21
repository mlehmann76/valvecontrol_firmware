/*
 * MainClass.h
 *
 *  Created on: 21.07.2020
 *      Author: marco
 */

#ifndef MAIN_MAINCLASS_H_
#define MAIN_MAINCLASS_H_

#include "WifiTask.h"
#include "controlTask.h"
#include "sht1x.h"
#include "mqttUserTask.h"
#include "mqtt_user_ota.h"
#include "status.h"
#include "sht1x.h"

class MainClass {
	WifiTask wifitask;
	ControlTask channel = {wifitask.eventGroup()};
	Sht1x sht1x = {GPIO_NUM_21, GPIO_NUM_22};
	mqtt::MqttOtaWorker mqttOta;
	mqtt::MqttUserTask mqttUser = {wifitask.eventGroup()};
	StatusTask status = {wifitask.eventGroup(), mqttUser.queue()};
public:
	int loop();
	static MainClass *instance() {
		static MainClass _inst;
		return &_inst;
	}
private:
	MainClass();
	virtual ~MainClass();
	void spiffsInit(void);
	int checkWPSButton();
};

#endif /* MAIN_MAINCLASS_H_ */
