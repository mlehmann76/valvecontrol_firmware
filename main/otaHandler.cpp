/*
 * otaHandler.cpp
 *
 *  Created on: 29.07.2020
 *      Author: marco
 */

#include "otaHandler.h"
#include "config_user.h"
#include "otaWorker.h"

#include <json.hpp>

#define TAG "MQTTOTA"

// using Json = cJson::Json;
using Json = nlohmann::json;

MqttOtaHandler::MqttOtaHandler(Ota::OtaWorker &_o, mqtt::MqttWorker &_m,
                               const std::string &_f, const std::string &_t)
    : m_ota(_o), m_messager(_m), m_firmwaretopic(_f), m_updatetopic(_t) {
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
    if (((event->topic_len == 0) && m_ota.isRunning()) ||
        (event->topic && event->topic == m_updatetopic)) {
        log_inst.info(TAG, "OTA update topic %s",
                       event->data_len < 64 ? event->data : "data");
        Ota::OtaPacket p(event->data, event->data_len);
        ret = m_ota.handle(p);
    } else if (event->topic &&
               event->topic ==
                   m_firmwaretopic.substr(0, m_firmwaretopic.length() - 2)) {
        log_inst.info(TAG, "%s",
                      std::string(event->topic, event->topic_len).c_str());
        Json root;

        Json firmware =
            nlohmann::json::parse(std::string(event->data, event->data_len),
            		nullptr, false, true);
        if (!firmware.is_discarded()) {
            handleFirmwareMessage(&firmware["firmware"]);
        } else {
        	log_inst.error(TAG, "message discarded (%.*s)",
        			event->data_len, event->data);
        }

        ret = 1;
    }
    return ret;
}

void MqttOtaHandler::handleFirmwareMessage(const Json *firmware) {
    int error = 0;
    Ota::OtaWorker::md5_update md5_update;

    if (firmware != nullptr && firmware->is_structured()) {
        auto update = readString((*firmware)["update"]);
        auto md5 = readString((*firmware)["md5"]);
        auto len = readNumber((*firmware)["len"]);

        if (update && update != "ota") {
            error = 1;
        }

        if (!error && !md5) {
            error = 2;
        }

        if (!error && (md5->length() != 32)) {
            error = 3;
        }

        if (!error && md5StrToAr(md5->c_str(), md5_update.md5) != 0) {
            error = 4;
        }

        if (!error && !len) {
            error = 5;
        } else {
            md5_update.len = *len;
        }

        if (error) {
            log_inst.error(TAG, "handleFirmwareMsg error %d", error);
        } else {
            log_inst.debug(
                TAG,
                "fileSize :%d ->md5: "
                "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x{:"
                "02x}%02x%02x%02x"
                "%02x%02x",
                md5_update.len, md5_update.md5[0], md5_update.md5[1],
                md5_update.md5[2], md5_update.md5[3], md5_update.md5[4],
                md5_update.md5[5], md5_update.md5[6], md5_update.md5[7],
                md5_update.md5[8], md5_update.md5[9], md5_update.md5[10],
                md5_update.md5[11], md5_update.md5[12], md5_update.md5[13],
                md5_update.md5[14], md5_update.md5[15]);

            m_ota.start(md5_update);
        }
    }
}

std::optional<std::string> MqttOtaHandler::readString(const Json &_item) {
    //	return _item.valid() ? std::optional<std::string>{_item.string()} :
    // std::nullopt;
    return _item.is_string()
               ? std::optional<std::string>{_item.get<std::string>()}
               : std::nullopt;
}

std::optional<long> MqttOtaHandler::readNumber(const Json &_item) {
    //	return _item.valid() ? std::optional<double>{_item.number()} :
    // std::nullopt;
    return _item.is_number() ? std::optional<long>{_item.get<long>()}
                                   : std::nullopt;
}
