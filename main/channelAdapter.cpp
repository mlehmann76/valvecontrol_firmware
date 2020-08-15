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
#include "esp_log.h"

#include "sdkconfig.h"
#include "config.h"
#include "config_user.h"
#include "channelAdapter.h"
#include "Json.h"

#include "MainClass.h"

#define TAG "channelAdapter"

bool isTopic(esp_mqtt_event_handle_t event, const char * pCommand) {
	const char* psTopic = event->topic;//event->topic_len >= strlen(mqttConf.getSubMsg()) ? &event->topic[strlen(mqttConf.getSubMsg()) - 2] : "";
	return strncmp(psTopic, pCommand, strlen(pCommand)) == 0;
}

int MqttChannelAdapter::onMessage(esp_mqtt_event_handle_t event) {
	if (event->data_len < 64) {
		ESP_LOGV(TAG, "Topic received!: (%d) %.*s (%d) %.*s", event->topic_len, event->topic_len, event->topic,
				event->data_len, event->data_len, event->data);
	} else {
		ESP_LOGV(TAG, "Topic received!: (%d) %.*s", event->topic_len, event->topic_len, event->topic);
	}
	//TODO check message and set channel or notify
	return 0;
}

void MqttChannelAdapter::onNotify(const ChannelBase*) {
	//TODO check message and notify
}

int MqttJsonChannelAdapter::onMessage(esp_mqtt_event_handle_t event) {
	int ret = 0;
	ESP_LOGV(TAG, "comparing %s == %s", event->topic, m_subtopic.c_str());
	if (event->topic != nullptr && event->topic == m_subtopic)	{
		ESP_LOGI(channel()->name(), "%.*s", event->topic_len, event->topic);

		int chanTime = -1;
		int status = -1;
		Json root;
		Json _chan = root.parse(event->data)["channel"][channel()->name()];
		ESP_LOGV(TAG, "_chan :%s", _chan.dump().c_str());
		if (_chan.valid()) {
			Json _val = _chan["val"];
			ESP_LOGV(TAG, "val :%s", _val.dump().c_str());

			if (_val.IsString()) {
				const char *pS = _val.string();
				status = ((strncmp(pS, "ON", 2) == 0) | (strncmp(pS, "on", 2) == 0)) ? 1 : 0;
			} else if (_val.IsNumber()) {
				status = (_val.number() != 0) ? 1 : 0;
			}
			Json timeObj = _chan["time"];
			if (timeObj.IsNumber()) {
				chanTime = timeObj.number();
			}

			ESP_LOGD(TAG, "channel :%s found ->%s, time : %d", channel()->name(), status == 1 ? "on" : "off", chanTime);

			if (status == -1) {
				channel()->notify();
			} else {
				channel()->set(status, std::chrono::seconds(chanTime));
			}
			ret = 1;
		}
	}
	return ret;
}

void MqttJsonChannelAdapter::onNotify(const ChannelBase* _b) {
		Json root; //keep link to root
		Json root1 = root.addObject("channel").addObject(_b->name()).addItem("val", Json(_b->get() ? "ON" : "OFF"));
		mqtt::MqttQueueType message(new mqtt::mqttMessage(m_pubtopic,root.dump()));
		MainClass::instance()->mqtt().send(std::move(message));
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
