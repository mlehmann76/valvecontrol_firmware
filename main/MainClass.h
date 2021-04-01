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
#include "WifiManager.h"
#include "otaWorker.h"
#include "sht1x.h"
#include <esp_event.h>
#include <freertos/event_groups.h>

class repository;
class MqttOtaHandler;
class MqttRepAdapter;
class ExclusiveAdapter;
class ChannelBase;
class Tasks;
class SntpSupport;
class StatusNotifyer;

namespace http {
class FileHandler;
class RepositoryHandler;
} // namespace http

namespace mqtt {
class MqttWorker;
} //namespace mqtt

class MainClass {

   public:
    MainClass();
    MainClass(const MainClass&) = delete;
    MainClass(MainClass&&) = delete;
    MainClass operator=(const MainClass&) = delete;
    MainClass operator=(MainClass&&) = delete;
    virtual ~MainClass() = default;

    void setup();
    int loop();
    void restart();
    static MainClass *instance() {
        static MainClass _inst;
        return &_inst;
    }
    mqtt::MqttWorker &mqtt() { return *_mqttUser; }
    EventGroupHandle_t &eventGroup() { return (main_event_group); }

  private:
    void checkWPSButton();
    void checkRestartButton();

    std::mutex mutex;
    std::condition_variable cvInitDone;
    std::shared_ptr<wifi::WifiManager> _wifi;
    Sht1x sht1x = {GPIO_NUM_21, GPIO_NUM_22};
    Ota::OtaWorker _otaWorker;
    std::shared_ptr<mqtt::MqttWorker> _mqttUser;
    std::shared_ptr<MqttOtaHandler> _mqttOtaHandler;
    std::shared_ptr<MqttRepAdapter> _configRepAdapter;
    std::shared_ptr<StatusNotifyer> _statusNotifyer;
    std::shared_ptr<SntpSupport> _sntp;
    std::shared_ptr<http::HttpServer> _http;
    std::shared_ptr<http::RepositoryHandler> _jsonHandler;
    std::shared_ptr<http::FileHandler> _appFileHandler;
    std::vector<std::shared_ptr<ChannelBase>> _channels;
    std::shared_ptr<ExclusiveAdapter> _cex; // only one channel should be active
    std::shared_ptr<Tasks> _tasks;
    EventGroupHandle_t main_event_group = nullptr;
    bool doExit;
    int m_wpsButtonCount = 0;
    int m_restartButtonCount = 0;
};

#endif /* MAIN_MAINCLASS_H_ */
