/*
 * MainClass.h
 *
 *  Created on: 21.07.2020
 *      Author: marco
 */

#ifndef MAIN_MAINCLASS_H_
#define MAIN_MAINCLASS_H_

#include <memory>
#include <vector>

#include "HttpServer.h"
#include "SemaphoreCPP.h"
#include "TimerCPP.h"
#include "WifiTask.h"
#include "mqttWorker.h"
#include "otaWorker.h"
#include "sht1x.h"

class repository;
class MqttOtaHandler;
class MqttRepAdapter;
class ExclusiveAdapter;
class ChannelBase;
class Tasks;
class SntpSupport;

namespace http {
	class FileHandler;
	class RepositoryHandler;
}

class MainClass {
    WifiTask wifitask;
    Sht1x sht1x = {GPIO_NUM_21, GPIO_NUM_22};
    Ota::OtaWorker otaWorker;
    mqtt::MqttWorker mqttUser;
    std::shared_ptr<SntpSupport> _sntp;
    std::shared_ptr<repository> _stateRepository;
    std::shared_ptr<repository> _controlRepository;
    std::shared_ptr<http::HttpServer> _http;
    std::shared_ptr<http::RepositoryHandler> _jsonHandler;
    std::shared_ptr<http::FileHandler> _spiffsHandler;
    std::shared_ptr<MqttOtaHandler> _mqttOtaHandler;
    std::shared_ptr<MqttRepAdapter> _controlRepAdapter;
    std::shared_ptr<MqttRepAdapter> _configRepAdapter;
    std::vector<std::shared_ptr<ChannelBase>> _channels;
    std::shared_ptr<ExclusiveAdapter> _cex; // only one channel should be active
    std::shared_ptr<Tasks> _tasks;

  public:
    void setup();
    int loop();
    static MainClass *instance() {
        static MainClass _inst;
        return &_inst;
    }
    EventGroupHandle_t &eventGroup() { return (wifitask.eventGroup()); }
    mqtt::MqttWorker &mqtt() { return mqttUser; }

  private:
    MainClass();
    virtual ~MainClass() = default;
    void spiffsInit(void);
};

#endif /* MAIN_MAINCLASS_H_ */
