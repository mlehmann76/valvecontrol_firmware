/*
 * MainClass.cpp
 *
 *  Created on: 21.07.2020
 *      Author: marco
 */

#include "MainClass.h"

#include <fmt/format.h>
#include <stdlib.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <thread>

#include "logger.h"
#include "HttpAuth.h"
#include "HttpServer.h"
#include "MqttRepAdapter.h"
#include "channelAdapter.h"
#include "channelFactory.h"
#include "config.h"
#include "config_user.h"
#include "echoServer.h"
#include "esp_event.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_wifi.h"
#include "esp_wps.h"
#include "nvs_flash.h"
#include "otaHandler.h"
#include "repository.h"
#include "sntp.h"
#include "statusrepository.h"
#include "tasks.h"

using namespace std::string_literals;

logger::Logger<	logger::DefaultLogPolicy, logger::ColoredOutput<
		logger::severity_type::debug,logger::severity_type::debug>> log_inst("") ;

#define TAG "MAIN"

MainClass::MainClass()
    : _sntp(std::make_shared<SntpSupport>()), _stateRepository(),
      _controlRepository(), _http(), _mqttOtaHandler(), _controlRepAdapter(),
      _configRepAdapter(), _channels(4),
      _cex(std::make_shared<ExclusiveAdapter>())

{
    mqttConf.init();
    _stateRepository = (std::make_shared<StatusRepository>(
        "/state", mqttUser, fmt::format("{}state", mqttConf.getDevName()),
        tag<DefaultLinkPolicy>{}));

    _controlRepository =
        (std::make_shared<repository>("/control", tag<DefaultLinkPolicy>{}));

    _http = (std::make_shared<http::HttpServer>(80));

    _jsonHandler = std::make_shared<http::RepositoryHandler>("GET", "/json");

    _mqttOtaHandler = (std::make_shared<MqttOtaHandler>(
        otaWorker, mqttUser, fmt::format("{}ota/#", mqttConf.getDevName()),
        fmt::format("{}ota/$implementation/binary", mqttConf.getDevName())));

    _controlRepAdapter = (std::make_shared<MqttRepAdapter>(
        *_controlRepository.get(), mqttUser,
        fmt::format("{}control", mqttConf.getDevName())));

    _configRepAdapter = (std::make_shared<MqttRepAdapter>(
        Config::repo(), mqttUser,
        fmt::format("{}config", mqttConf.getDevName())));

    _tasks = std::make_shared<Tasks>(Config::repo(), *_stateRepository,
                                     *_controlRepository);
}

void MainClass::setup() {

    spiffsInit();

    _sntp->init();
    _tasks->setup();
    mqttUser.init();

    wifitask.addConnectionObserver(_http->obs());
    wifitask.addConnectionObserver(mqttUser.obs());

    _jsonHandler->add("/json/state.json", *_stateRepository);
    _jsonHandler->add("/json/config.json", Config::repo());
    _jsonHandler->add("/json/command.json", *_controlRepository);

    http::HttpAuth::AuthToken _token;
    _token.pass = "admin";
    _token.user = "admin";
    _http->addPathHandler(std::make_shared<http::HttpAuth>(
        _jsonHandler.get(), _token, http::HttpAuth::DIGEST_AUTH_SHA256_MD5));

    for (size_t i = 0; i < _channels.size(); i++) {
        _channels[i] = std::shared_ptr<ChannelBase>(
            LedcChannelFactory::channel(i, chanConf.getTime(i)));
        _cex->setChannel(&*_channels[i]);
        _stateRepository->create("actors/" + _channels[i]->name(),
                                 {{{"value", "OFF"s}}});

        _controlRepository
            ->create("actors/" + _channels[i]->name(), {{{"value", "OFF"s}}})
            .set([=](const property &p) {
                auto it = p.find("value");
                if (it != p.end() && it->second.is<StringType>()) {
                    std::string s = it->second.get_unchecked<StringType>();
                    _channels[i]->set(s == "on" || s == "ON" || s == "On",
                                      chanConf.getTime(i));
                }
            });

        _channels[i]->add([=](ChannelBase *b) { _cex->onNotify(b); });
        _channels[i]->add([=](ChannelBase *b) {
            _stateRepository->set("actors/" + b->name(),
                                  {{"value", b->get() ? "ON"s : "OFF"s}});
        });
    }

    sht1x.regProperty(_stateRepository.get(), "sensors/sht1x");
}

void MainClass::spiffsInit(void) {
    log_inst.info(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {.base_path = "/spiffs",
                                  .partition_label = NULL,
                                  .max_files = 5,
                                  .format_if_mount_failed = false};

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
        	log_inst.error(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
        	log_inst.error(TAG, "Failed to find SPIFFS partition");
        } else {
        	log_inst.error(TAG, "Failed to initialize SPIFFS {}",
                     esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
    	log_inst.error(TAG, "Failed to get SPIFFS partition information {}",
                 esp_err_to_name(ret));
    } else {
    	log_inst.info(TAG, "Partition size: total: {:d}, used: {:d}", total, used);
    }
}

int MainClass::loop() {
    int count = 0;
    uint32_t heapFree = 0;
    std::unique_ptr<char[]> pcWriteBuffer(new char[2048]);

    while (1) {
        // check for time update by _sntp
        if (!(xEventGroupGetBits(MainClass::instance()->eventGroup()) &
              SNTP_UPDATED) &&
            _sntp->update()) {
            xEventGroupSetBits(MainClass::instance()->eventGroup(),
                               SNTP_UPDATED);
        }

        if (0 == count) {
            if (esp_get_free_heap_size() != heapFree) {
                heapFree = esp_get_free_heap_size();
                vTaskGetRunTimeStats(pcWriteBuffer.get());
                //				ESP_LOGI(TAG, "[APP] Free
                //memory: %d bytes\n",
                // esp_get_free_heap_size());
                // ESP_LOGI(TAG,
                // "%s\n", _controlRepository->debug().c_str());
                // ESP_LOGI(TAG, "%s\n", _stateRepository->debug().c_str());
                count = 500;
            }
        } else {
            count--;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
