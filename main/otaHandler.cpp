/*
 * otaHandler.cpp
 *
 *  Created on: 29.07.2020
 *      Author: marco
 */

#include "config_user.h"
#include "esp_log.h"
#include "otaWorker.h"
#include "otaHandler.h"
#include "Json.h"

#define TAG "MQTTOTA"

MqttOtaHandler::MqttOtaHandler(Ota::OtaWorker &_o, mqtt::MqttWorker &_m, const std::string &_f, const std::string &_t) :
		m_ota(_o), m_messager(_m), m_firmwaretopic(_f), m_updatetopic(_t) {
	m_messager.addHandle(this);
}

int MqttOtaHandler::md5StrToAr(const char *pMD5, uint8_t *md5) {
	int error = 0;
	for (int i = 0; i < 32; i++) {
		int temp = 0;
		if (pMD5[i] <= '9' && pMD5[i] >= '0') {
			temp = pMD5[i] - '0';
		} else if (pMD5[i] <= 'f' && pMD5[i] >= 'a') {
			temp = pMD5[i] - 'a' + 10;
		} else if (pMD5[i] <= 'F' && pMD5[i] >= 'A') {
			temp = pMD5[i] - 'A' + 10;
		} else {
			error = 1;
			break;
		}

		if ((i & 1) == 0) {
			temp *= 16;
			md5[i / 2] = temp;
		} else {
			md5[i / 2] += temp;
		}
	}
	return error;
}

int MqttOtaHandler::onMessage(esp_mqtt_event_handle_t event) {
	int ret = 0;
	if (((event->topic_len == 0) && m_ota.isRunning()) || (event->topic && event->topic == m_updatetopic)) {
		ESP_LOGE(TAG, "OTA update topic %s", event->data_len < 64 ? event->data : "data");
		Ota::OtaPacket p(event->data, event->data_len);
		ret = m_ota.handle(p);
	}else if (event->topic && event->topic == m_firmwaretopic.substr(0, m_firmwaretopic.length()-2)) {
		ESP_LOGI(TAG, "%.*s", event->topic_len, event->topic);
		Json root;

		Json firmware = root.parse(event->data)["firmware"];
		if (firmware.valid()) {
			handleFirmwareMessage(&firmware);
		}

		ret = 1;
	}
	return ret;
}

void MqttOtaHandler::handleFirmwareMessage(const Json* firmware) {
	int error = 0;
	Ota::OtaWorker::md5_update md5_update;
	const char* pMD5 = NULL;

	if (firmware != nullptr && firmware->valid()) {
		const char* pUpdate = (*firmware)["update"].string();
		if (strcmp(pUpdate, "ota") != 0) {
			error = 1;
		}

		if (!error && (*firmware)["md5"].valid()) {
			error = 2;
		} else {
			pMD5 = (*firmware)["md5"].string();
		}

		if (!error && (strlen(pMD5) != 32)) {
			error = 3;
		}

		if (!error && md5StrToAr(pMD5, md5_update.md5) != 0) {
			error = 4;
		}

		if (!error && (*firmware)["len"].valid()) {
			error = 5;
		} else {
			md5_update.len = (*firmware)["len"].number();
		}

		if (error) {
			ESP_LOGE(TAG, "handleFirmwareMsg error %d", error);
		} else {
			ESP_LOGI(TAG, "fileSize :%d ->md5: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
					md5_update.len, md5_update.md5[0], md5_update.md5[1], md5_update.md5[2], md5_update.md5[3],
					md5_update.md5[4], md5_update.md5[5], md5_update.md5[6], md5_update.md5[7], md5_update.md5[8],
					md5_update.md5[9], md5_update.md5[10], md5_update.md5[11], md5_update.md5[12], md5_update.md5[13],
					md5_update.md5[14], md5_update.md5[15]);

			m_ota.start(md5_update);
		}
	}
}
