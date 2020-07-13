/*
 * mqtt_user_ota.h
 *
 *  Created on: 29.06.2018
 *      Author: marco
 */

#ifndef MQTT_USER_OTA_H_
#define MQTT_USER_OTA_H_

#include "mbedtls/md5.h"
#include "esp_ota_ops.h"

namespace mqtt {

class Decoder {
public:
	virtual ~Decoder() {}
	virtual void init() = 0;
	virtual int decode(const uint8_t *src, size_t len) = 0;
	virtual const uint8_t* data() const = 0;
	virtual size_t len() = 0;
};

class B85decode : public Decoder{
	static const size_t DECODEBUFSIZE = (CONFIG_MQTT_BUFFER_SIZE + 5);
	uint32_t _decodePos;
	uint32_t _len;
	uint32_t _sumLen;
	uint8_t buffer[DECODEBUFSIZE];
	uint8_t decoded[DECODEBUFSIZE];
public:
	void init();
	int decode(const uint8_t *src, size_t len);
	size_t len() {return _len; }
	size_t sum() {return _sumLen;}
	const uint8_t* data() const { return decoded; }
};


class MqttOtaWorker {
	enum ota_state_e {
		OTA_IDLE, OTA_START, OTA_DATA, OTA_FINISH, OTA_ERROR
	};
	struct md5_update {
		uint8_t md5[16];
		size_t len;
	};

public:
	MqttOtaWorker();
	int handle(const char * p, esp_mqtt_event_handle_t event);
	bool isRunning() const { return m_isRunning; }
	void start(const md5_update&);

private:
	void task();
	void otaStart();
	void otaFinish();
	void otaData();

	TimerMember<MqttOtaWorker> m_timeout;
	ota_state_e m_ota_state = OTA_IDLE;
	mbedtls_md5_context m_md5ctx;
	Decoder *m_decodeCtx;
	md5_update m_md5Updata;
	/* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
	esp_ota_handle_t update_handle = 0;
	const esp_partition_t *update_partition = NULL;
	bool m_isRunning = false;
	size_t sum = 0;
};

}

#endif /* MQTT_USER_OTA_H_ */
