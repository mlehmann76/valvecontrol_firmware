/*
 * otaHandler.h
 *
 *  Created on: 29.07.2020
 *      Author: marco
 */

#ifndef MAIN_OTAHANDLER_H_
#define MAIN_OTAHANDLER_H_

#include <string>
#include "cJSON.h"
#include "mqtt_client.h"
#include "mqttWorker.h"

namespace Ota {
class OtaWorker;
}
class Messager;

class MqttOtaHandler : public mqtt::AbstractMqttReceiver {
public:
	MqttOtaHandler(Ota::OtaWorker &_o, mqtt::MqttWorker &_m, const std::string &_f, const std::string &_t);
	int onMessage(esp_mqtt_event_handle_t event);
private:
	static int md5StrToAr(char* pMD5, uint8_t* md5);
	void handleFirmwareMessage(cJSON* firmware);
	Ota::OtaWorker &m_ota;
	mqtt::MqttWorker &m_messager;
	std::string m_firmwaretopic;
	std::string m_updatetopic;
};


#endif /* MAIN_OTAHANDLER_H_ */
