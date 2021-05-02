/*
 * MainClass.cpp
 *
 *  Created on: 21.07.2020
 *      Author: marco
 */

#include "MainClass.h"

#include <stdlib.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <thread>

#include "FileHandler.h"
#include "HttpAuth.h"
#include "HttpServer.h"
#include "LedFlasher.h"
#include "MqttRepAdapter.h"
#include "RepositoryHandler.h"
#include "WifiManager.h"
#include "channelAdapter.h"
#include "channelFactory.h"
#include "config.h"
#include "config_user.h"
#include "echoServer.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "libespfs/espfs.h"
#include "libespfs/vfs.h"
#include "logger.h"
#include "mqttWorker.h"
#include "otaHandler.h"
#include "repository.h"
#include "sntp.h"
#include "statusnotifyer.h"
#include "tasks.h"

using namespace std::string_literals;

logType log_inst({}, {});

#define TAG "MAIN"

extern const uint8_t espfs_bin[];

MainClass::MainClass()
    : _sntp(std::make_shared<SntpSupport>()), _channels(4),
      _cex(std::make_shared<ExclusiveAdapter>()), doExit(false) {}

void MainClass::setup() {

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    main_event_group = xEventGroupCreate();

    espfs_config_t espfs_config = {.addr = espfs_bin, .part_label = "espfs"};

    espfs_fs_t *fs = espfs_init(&espfs_config);
    assert(fs != NULL);

    esp_vfs_espfs_conf_t vfs_espfs_conf = {
        .base_path = "/espfs",
        .overlay_path = NULL,
        .fs = fs,
        .max_files = 5,
    };

    ESP_ERROR_CHECK(esp_vfs_espfs_register(&vfs_espfs_conf));

    log_inst.setLogSeverity("I2C", logger::severity_type::warning);
    log_inst.setLogSeverity("repository", logger::severity_type::info);
    log_inst.setLogSeverity("RepositoryHandler", logger::severity_type::info);
    log_inst.setLogSeverity("MQTTS", logger::severity_type::error);
    log_inst.setLogSeverity("MQTTOTA", logger::severity_type::error);

    baseConf.init();
    sysConf.init();
    mqttConf.init();
    netConf.init();

    _led1 = std::make_shared<LedFlasher>(LED_YELLOW_GPIO_PIN, false);
    _led2 = std::make_shared<LedFlasher>(LED_GREEN_GPIO_PIN, false);

    _wifi = std::make_shared<wifi::WifiManager>(Config::repo(), mutex,
                                                cvInitDone, _led1);
    {
        std::unique_lock<std::mutex> lck(mutex);
        cvInitDone.wait(lck, [this] { return _wifi->initDone(); });
    }

    if (mqttConf.enabled()) {
        _mqttUser = std::make_shared<mqtt::MqttWorker>(_led2);

        _mqttOtaHandler = (std::make_shared<MqttOtaHandler>(
            _otaWorker, *_mqttUser,
            utilities::string_format("%sota/#", mqttConf.getDevName().c_str()),
            utilities::string_format("%sota/$implementation/binary",
                        mqttConf.getDevName().c_str())));

        _configRepAdapter = (std::make_shared<MqttRepAdapter>(
            Config::repo(), *_mqttUser,
            utilities::string_format("%sconfig", mqttConf.getDevName().c_str())));

        _wifi->addConnectionObserver(_mqttUser->obs());

        _mqttUser->init();

        _statusNotifyer = std::make_shared<StatusNotifyer>(
            Config::repo(), *_mqttUser,
            utilities::string_format("%sstate", mqttConf.getDevName().c_str()));

        Config::repo().addNotify("/*/*/state", onStateNotify(*_statusNotifyer));
    }

    _tasks = std::make_shared<Tasks>(Config::repo());

    _sntp->init();

    _tasks->setup();

    //_wifi->addConnectionObserver(_http->obs());
    _http = (std::make_shared<http::HttpServer>(80));
    _http->start();

    _jsonHandler =
        std::make_shared<http::RepositoryHandler>("GET,POST,DELETE", "/json");
    _appFileHandler = std::make_shared<http::FileHandler>("GET", "/", "/espfs");

    _jsonHandler->add("/json", Config::repo());

    _http->addPathHandler(_appFileHandler);

    http::HttpAuth::AuthToken _token = {sysConf.getUser(), sysConf.getPass()};
    _http->addPathHandler(std::make_shared<http::HttpAuth>(
        _jsonHandler.get(), _token, http::HttpAuth::BASIC_AUTH));

    for (size_t i = 0; i < _channels.size(); i++) {
        _channels[i] = std::shared_ptr<ChannelBase>(
            LedcChannelFactory::channel(i, chanConf.getTime(i)));
        _cex->setChannel(&*_channels[i]);
        Config::repo().create("/actors/" + _channels[i]->name() + "/state",
                              {{{"value", "OFF"s}}});

        Config::repo()
            .create("/actors/" + _channels[i]->name() + "/control",
                    {{{"value", "OFF"s}}})
            .set([=](const property &p) -> std::optional<property> {
                auto it = p.find("value");
                if (it != p.end() && it->second.is<StringType>()) {
                    std::string s = it->second.get_unchecked<StringType>();
                    _channels[i]->set(s == "on" || s == "ON" || s == "On",
                                      chanConf.getTime(i));
                }
                return {};
            });

        _channels[i]->add([=](ChannelBase *b) -> std::optional<property> {
            _cex->onNotify(b);
            return {};
        });
        _channels[i]->add([=](ChannelBase *b) -> std::optional<property> {
            Config::repo().set("/actors/" + b->name() + "/state",
                               {{"value", b->get() ? "ON"s : "OFF"s}});
            return {};
        });
    }

    sht1x.regProperty(&Config::repo(), "/sensors/sht1x/state");

    Config::repo()
        .create("/system/base/control/restart", {{{"start", false}}})
        .set([this](const property &) -> std::optional<property> {
            this->restart();
            return {};
        });

    log_inst.setLogSeverity("repository", logger::severity_type::debug);
}

void MainClass::restart() { doExit = true; }

void MainClass::checkWPSButton() {
#ifdef WPS_BUTTON
    if ((gpio_get_level((gpio_num_t)WPS_BUTTON) == 0)) {
        m_wpsButtonCount++;
        if (m_wpsButtonCount > (WPS_LONG_MS / portTICK_PERIOD_MS)) {
            _wifi->startWPS();
            m_wpsButtonCount = 0;
        }
    }
#endif
}

void MainClass::checkRestartButton() {
#ifdef RESTART_BUTTON
    if ((gpio_get_level((gpio_num_t)RESTART_BUTTON) == 0)) {
        m_restartButtonCount++;
    } else {
        if (m_restartButtonCount > (RESTART_LONG_MS / portTICK_PERIOD_MS)) {
            baseConf.resetToDefault();
        } else if (m_restartButtonCount >
                   (RESTART_SHORT_MS / portTICK_PERIOD_MS)) {
            restart();
        }
        m_restartButtonCount = 0;
    }
#endif
}

int MainClass::loop() {
    int count = 0;
    uint32_t heapFree = 0;
    std::unique_ptr<char[]> pcWriteBuffer(new char[2048]);

    while (!doExit) {
        // check for time update by _sntp
        if (!(xEventGroupGetBits(eventGroup()) & SNTP_UPDATED) &&
            _sntp->update()) {
            xEventGroupSetBits(eventGroup(), SNTP_UPDATED);
        }

        if (0 == count) {
            if (esp_get_free_heap_size() != heapFree) {
                heapFree = esp_get_free_heap_size();
#if 0
                vTaskGetRunTimeStats(pcWriteBuffer.get());
                log_inst.info(TAG, "[APP] Free memory: %d bytes\n{:s}",
                              esp_get_free_heap_size(),
							  std::string(pcWriteBuffer.get()));
#else
                log_inst.info(TAG, "[APP] Free memory: %d bytes",
                              esp_get_free_heap_size());
#endif
                count = 500;
            }
        } else {
            count--;
        }

        checkWPSButton();
        checkRestartButton();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::this_thread::yield();
    }
    return 0;
}
