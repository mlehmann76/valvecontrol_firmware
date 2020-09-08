/*
 * ChannelAdapter.cpp
 *
 *  Created on: 23.07.2020
 *      Author: marco
 */

#include <stdio.h>
#include <chrono>
#include <cassert>
#include <memory>
#include <string>
#include "esp_log.h"

#include "sdkconfig.h"
#include "config.h"
#include "config_user.h"
#include "channelAdapter.h"
#include "json.hpp"

#include "MainClass.h"

#define TAG "channelAdapter"

bool isTopic(esp_mqtt_event_handle_t event, const char * pCommand) {
	const char* psTopic = event->topic;//event->topic_len >= strlen(mqttConf.getSubMsg()) ? &event->topic[strlen(mqttConf.getSubMsg()) - 2] : "";
	return strncmp(psTopic, pCommand, strlen(pCommand)) == 0;
}

ChannelBase* ExclusiveAdapter::channel() {
	return nullptr; // never used
}

void ExclusiveAdapter::setChannel(ChannelBase *_channel) {
	m_vchannel.push_back(_channel);
}

void ExclusiveAdapter::onNotify(const ChannelBase* _c) {
	if (_c->get()) {
		for (auto c : m_vchannel) {
			//check, if another channel is enabled and disable this channel
			if (c != _c && c->get()) {
				c->set(false,std::chrono::seconds(-1));
			}
		}
	}
}
