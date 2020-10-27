/*
 * otaHandler.h
 *
 *  Created on: 29.07.2020
 *      Author: marco
 */

#ifndef MAIN_OTAHANDLER_H_
#define MAIN_OTAHANDLER_H_

#include "mqttWorker.h"
#include "mqtt_client.h"
#include <string>
#include <optional>
#include <json.hpp>

using Json = nlohmann::json;

namespace Ota {
class OtaWorker;
}
class Messager;

class MqttOtaHandler : public mqtt::AbstractMqttReceiver {
  public:
    MqttOtaHandler(Ota::OtaWorker &_o, mqtt::MqttWorker &_m,
                   const std::string &_f, const std::string &_t);
    virtual int onMessage(esp_mqtt_event_handle_t event);
    virtual std::string topic() { return m_firmwaretopic; }

  private:
    static int md5StrToAr(const char *pMD5, uint8_t *md5);
    void handleFirmwareMessage(const Json *firmware);
    std::optional<std::string> readString(const Json &_item);
    std::optional<double> readDouble(const Json &_item);

    Ota::OtaWorker &m_ota;
    mqtt::MqttWorker &m_messager;
    std::string m_firmwaretopic;
    std::string m_updatetopic;
};

#endif /* MAIN_OTAHANDLER_H_ */
