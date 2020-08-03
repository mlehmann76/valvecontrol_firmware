/*
 * messageHandler.cpp
 *
 *  Created on: 20.07.2020
 *      Author: marco
 */

#include "messager.h"

#include <string.h>
#include <stdlib.h>

#include "sdkconfig.h"
#include "esp_log.h"
#include "channelAdapter.h"

static const char *TAG = "MESSAGER";



Messager::Messager() {
	// TODO Auto-generated constructor stub
}

Messager::~Messager() {
	// TODO Auto-generated destructor stub
}

void Messager::handle(esp_mqtt_event_handle_t event) {
	if (event->data_len < 64) {
		ESP_LOGD(TAG, "Topic received!: (%d) %.*s (%d) %.*s", event->topic_len, event->topic_len, event->topic,
				event->data_len, event->data_len, event->data);
	} else {
		ESP_LOGD(TAG, "Topic received!: (%d) %.*s", event->topic_len, event->topic_len, event->topic);
	}

	int ret = 0;
	//provide message to last handler for accelerated test
	if (m_lastMqttRec != nullptr) {
		ESP_LOGD(TAG, "Forwarding ...");
		ret = m_lastMqttRec->onMessage(event);
	}
	if (!ret) { //not handled yet
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

void Messager::addHandle(AbstractMqttReceiver* _a) {
	m_mqttRec.push_back(_a);
}


/*
int handleConfigMsg(const char * topic, esp_mqtt_event_handle_t event) {
	int ret = 0;
	if (isTopic(event, topic)) {
		ESP_LOGI(TAG, "%.*s", event->topic_len, event->topic);
		//FIXME updateConfig(event->data);
		ret = 1;
	}
	return ret;
}
*/
/* */


