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

#include "MainClass.h"
#include "esp_log.h"
#include "otaWorker.h"

static const char *TAG = "MQTTS";

namespace mqtt {

esp_err_t MqttWorker::mqtt_event_handler(esp_mqtt_event_handle_t event) {
    esp_mqtt_client_handle_t client = event->client;
    MqttWorker *mqtt = reinterpret_cast<MqttWorker *>(event->user_context);
    int msg_id;
    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGD(TAG, "MQTT_EVENT_CONNECTED");
        xEventGroupSetBits(MainClass::instance()->eventGroup(),
                           MQTT_CONNECTED_BIT);
        for (auto _s : mqtt->subTopics()) {
            msg_id = esp_mqtt_client_subscribe(client, _s.c_str(), 1);
            ESP_LOGD(TAG, "sent subscribe %s successful, msg_id=%d", _s.c_str(),
                     msg_id);
        }
        mqtt->isMqttConnected = true;
        break;
    case MQTT_EVENT_DISCONNECTED:
        xEventGroupClearBits(MainClass::instance()->eventGroup(),
                             MQTT_CONNECTED_BIT);
        ESP_LOGD(TAG, "MQTT_EVENT_DISCONNECTED");
        mqtt->isMqttConnected = false;
        mqtt->isMqttInit = false;
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGD(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGD(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGD(TAG, "MQTT_EVENT_DATA");
        mqtt->handle(event);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
        break;
    case MQTT_EVENT_BEFORE_CONNECT:
        ESP_LOGE(TAG, "MQTT_EVENT_BEFORE_CONNECT");
        break;
    case MQTT_EVENT_ANY:
        ESP_LOGE(TAG, "MQTT_EVENT_ANY");
    }
    return ESP_OK;
}

void MqttWorker::send(MqttQueueType rx) {
    send(rx.get());
    // m_pubQueue.add((rx),
    // std::chrono::duration_cast<portTick>(std::chrono::milliseconds(10)).count());
}

void MqttWorker::handle(esp_mqtt_event_handle_t event) {
    if (event->data_len < 64) {
        ESP_LOGD(TAG, "Topic received!: (%d) %.*s (%d) %.*s", event->topic_len,
                 event->topic_len, event->topic, event->data_len,
                 event->data_len, event->data);
    } else {
        ESP_LOGD(TAG, "Topic received!: (%d) %.*s", event->topic_len,
                 event->topic_len, event->topic);
    }

    int ret = 0;
    // provide message to last handler for accelerated test
    if (m_lastMqttRec != nullptr) {
        ESP_LOGD(TAG, "Forwarding ...");
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
}

void MqttWorker::addHandle(AbstractMqttReceiver *_a) {
    m_mqttRec.push_back(_a);
    auto search = m_subtopics.find(_a->topic());
    if (search == m_subtopics.end()) {
        m_subtopics.insert(_a->topic());
        ESP_LOGV(TAG, "addHandle, add topic = %s", _a->topic().c_str());
        if (isMqttConnected) {
            int msg_id =
                esp_mqtt_client_subscribe(client, _a->topic().c_str(), 1);
            ESP_LOGD(TAG, "sent subscribe %s successful, msg_id=%d",
                     _a->topic().c_str(), msg_id);
        }
    } else {
        ESP_LOGV(TAG, "addHandle, topic %s already registered",
                 _a->topic().c_str());
    }
}

void MqttWorker::send(mqttMessage *rxData) {
    if ((client != NULL) && isMqttConnected) {
        ESP_LOGD(TAG, "publish %.*s : %.*s", rxData->m_topic.length(),
                 rxData->m_topic.c_str(), rxData->m_data.length(),
                 rxData->m_data.c_str());
        int msg_id = esp_mqtt_client_publish(client, rxData->m_topic.c_str(),
                                             rxData->m_data.c_str(),
                                             rxData->m_data.length() + 1, 1, 0);
        ESP_LOGD(TAG, "sent publish successful, msg_id=%d", msg_id);
    }
}

void MqttWorker::init(void) {

    m_server = Config::repo().get<std::string>("mqtt", "server");
    m_user = Config::repo().get<std::string>("mqtt", "user");
    m_pass = Config::repo().get<std::string>("mqtt", "pass");

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
        ESP_LOGD(TAG, "starting client");
        esp_mqtt_client_start(client);
    } else {
        ESP_LOGE(TAG, "mqtt connect failed, client not initialized");
    }
}

void MqttWorker::disconnect(void) {
    ESP_LOGE(TAG, "stopping client");

    if (client != NULL)
        esp_mqtt_client_stop(client);
}

} // namespace mqtt
