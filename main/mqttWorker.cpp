/*
 * mqtt_client.c
 *
 *  Created on: 28.06.2018
 *      Author: marco
 */

#include "mqttWorker.h"

#include <algorithm>
#include <stdlib.h>
#include <string.h>

#include "sdkconfig.h"

#include "QueueCPP.h"
#include "SemaphoreCPP.h"
#include "TimerCPP.h"
#include "freertos/event_groups.h"

#include "config.h"
#include "config_user.h"

#include "LedFlasher.h"
#include "MainClass.h"
#include "otaWorker.h"

static const char *TAG = "MQTTS";

namespace mqtt {

esp_err_t MqttWorker::mqtt_event_handler(esp_mqtt_event_handle_t event) {
    esp_mqtt_client_handle_t client = event->client;
    MqttWorker *mqtt = reinterpret_cast<MqttWorker *>(event->user_context);
    int msg_id;
    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        log_inst.debug(TAG, "MQTT_EVENT_CONNECTED");

        for (auto _s : mqtt->subTopics()) {
            msg_id = esp_mqtt_client_subscribe(client, _s.c_str(), 1);
            log_inst.debug(TAG, "sent subscribe %s successful, msg_id=%d",
                           _s.c_str(), msg_id);
        }
        mqtt->isMqttConnected = true;
        mqttConf.setConnected(true);
        break;
    case MQTT_EVENT_DISCONNECTED:
        log_inst.debug(TAG, "MQTT_EVENT_DISCONNECTED");
        mqtt->isMqttConnected = false;
        mqtt->isMqttInit = false;
        mqttConf.setConnected(false);
        break;
    case MQTT_EVENT_SUBSCRIBED:
        log_inst.debug(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        log_inst.debug(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d",
                       event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        log_inst.debug(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        log_inst.debug(TAG, "MQTT_EVENT_DATA");
        mqtt->handle(event);
        break;
    case MQTT_EVENT_ERROR:
        log_inst.error(TAG, "MQTT_EVENT_ERROR");
        break;
    case MQTT_EVENT_BEFORE_CONNECT:
        log_inst.debug(TAG, "MQTT_EVENT_BEFORE_CONNECT");
        break;
    case MQTT_EVENT_ANY:
        log_inst.info(TAG, "MQTT_EVENT_ANY");
    }
    return ESP_OK;
}

void MqttWorker::send(MqttQueueType rx) {
    send(rx.get());
    // m_pubQueue.add((rx),
    // std::chrono::duration_cast<portTick>(std::chrono::milliseconds(10)).count());
}

void MqttWorker::handle(esp_mqtt_event_handle_t event) {
    //    if (event->topic != nullptr && event->data != nullptr) {
    //        if (event->data_len < 64) {
    //            log_inst.debug(
    //                TAG, "Topic received!: (%d) {} (%d) {}",
    //                event->topic_len, std::string(event->topic,
    //                event->topic_len), event->data_len,
    //                std::string(event->data, event->data_len));
    //        } else {
    //            log_inst.debug(TAG, "Topic received!: (%d) {}",
    //            event->topic_len,
    //                           std::string(event->topic, event->topic_len));
    //        }
    //    }

    m_led->set(LedFlasher::ON);
    int ret = 0;
    // provide message to last handler for accelerated test
    if (m_lastMqttRec != nullptr) {
        log_inst.debug(TAG, "Forwarding ...");
        ret = m_lastMqttRec->onMessage(event);
    }
    if (!ret) { // not handled yet
        m_lastMqttRec = nullptr;
        for (auto m : m_mqttRec) {
            ret = m->onMessage(event);
            if (ret) {
                m_lastMqttRec = m;
                break;
            }
        }
    }
    m_led->set(LedFlasher::OFF);
}

void MqttWorker::addHandle(AbstractMqttReceiver *_a) {
    m_mqttRec.push_back(_a);
    auto search = m_subtopics.find(_a->topic());
    if (search == m_subtopics.end()) {
        m_subtopics.insert(_a->topic());
        log_inst.debug(TAG, "addHandle, add topic = {}", _a->topic());
        if (isMqttConnected) {
            int msg_id =
                esp_mqtt_client_subscribe(client, _a->topic().c_str(), 1);
            log_inst.debug(TAG, "sent subscribe %s successful, msg_id=%d",
                           _a->topic().c_str(), msg_id);
        }
    } else {
        log_inst.debug(TAG, "addHandle, topic %s already registered",
                       _a->topic().c_str());
    }
}

void MqttWorker::send(mqttMessage *rxData) {
    if ((client != NULL) && isMqttConnected) {
        // log_inst.debug(TAG, "publish {:s} :
        // {:s}",rxData->m_topic,rxData->m_data);
        int msg_id = esp_mqtt_client_publish(client, rxData->m_topic.c_str(),
                                             rxData->m_data.c_str(),
                                             rxData->m_data.length() + 1, 1, 0);
        log_inst.debug(TAG, "sent publish successful, msg_id=%d", msg_id);
    }
}

void MqttWorker::init(void) {

    m_server = mqttConf.getServer();
    m_user = mqttConf.getUser();
    m_pass = mqttConf.getPass();

    esp_mqtt_client_config_t mqtt_cfg;
    memset(&mqtt_cfg, 0, sizeof(mqtt_cfg));
    mqtt_cfg.uri = m_server.c_str();
    mqtt_cfg.event_handle = mqtt_event_handler;
    mqtt_cfg.username = m_user.c_str();
    mqtt_cfg.password = m_pass.c_str();
    mqtt_cfg.user_context = this;

    client = esp_mqtt_client_init(&mqtt_cfg);
}

void MqttWorker::connect(void) {
    if (client != NULL) {
        log_inst.debug(TAG, "starting client");
        esp_mqtt_client_start(client);
    } else {
        log_inst.error(TAG, "mqtt connect failed, client not initialized");
    }
}

void MqttWorker::disconnect(void) {
    log_inst.debug(TAG, "stopping client");

    if (client != NULL)
        esp_mqtt_client_stop(client);
}

} // namespace mqtt
