/*
 * MainClass.h
 *
 *  Created on: 21.07.2020
 *      Author: marco
 */

#ifndef MAIN_MAINCLASS_H_
#define MAIN_MAINCLASS_H_

#include <vector>
#include "mqttWorker.h"
#include "TimerCPP.h"
#include "SemaphoreCPP.h"
#include "WifiTask.h"
#include "sht1x.h"
#include "otaWorker.h"
#include "HttpServer.h"
#include "RepositoryHandler.h"

class repository;
class MqttOtaHandler;
class MqttRepAdapter;
class ExclusiveAdapter;
class ChannelBase;

class MainClass {
	WifiTask wifitask;
	Sht1x sht1x = {GPIO_NUM_21, GPIO_NUM_22};
	Ota::OtaWorker otaWorker;
	mqtt::MqttWorker mqttUser;
	std::shared_ptr<SntpSupport> sntp;
	std::shared_ptr<repository> _stateRepository;
	std::shared_ptr<repository> _controlRepository;
	std::shared_ptr<http::HttpServer> _http;
	std::shared_ptr<http::RepositoryHandler> _jsonHandler;
	std::shared_ptr<MqttOtaHandler> _mqttOtaHandler;
	std::shared_ptr<MqttRepAdapter> _controlRepAdapter;
	std::shared_ptr<MqttRepAdapter> _configRepAdapter;
	std::vector<std::shared_ptr<ChannelBase>> _channels;
	std::shared_ptr<ExclusiveAdapter> _cex; //only one channel should be active


public:
	void setup();
	int loop();
	static MainClass *instance() {
		static MainClass _inst;
		return &_inst;
	}
	EventGroupHandle_t& eventGroup() {return (wifitask.eventGroup());}
	mqtt::MqttWorker& mqtt() {return mqttUser;}
private:
	MainClass();
	virtual ~MainClass() = default;
	void spiffsInit(void);
};

#endif /* MAIN_MAINCLASS_H_ */
