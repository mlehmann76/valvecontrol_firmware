/*
 * MqttRepAdapter.cpp
 *
 *  Created on: 05.09.2020
 *      Author: marco
 */

#include "MqttRepAdapter.h"

MqttRepAdapter::MqttRepAdapter(repository &_r, mqtt::MqttWorker &_m,
                               const std::string &_t)
    : m_repo(&_r), m_mqtt(&_m), m_topic(_t) {
    m_mqtt->addHandle(this);
}

int MqttRepAdapter::onMessage(esp_mqtt_event_handle_t event) {
    int ret = 0;
    if ((event->topic_len) && (event->topic && event->topic == m_topic)) {
        std::string message(event->data, event->data_len);
        m_repo->parse(message);
        ret = 1;
    }
    return ret;
}
