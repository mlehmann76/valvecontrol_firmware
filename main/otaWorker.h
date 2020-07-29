/*
 * mqtt_user_ota.h
 *
 *  Created on: 29.06.2018
 *      Author: marco
 */

#ifndef MQTT_USER_OTA_H_
#define MQTT_USER_OTA_H_

#include <string>
#include <vector>
#include "TimerCPP.h"
#include "mbedtls/md5.h"
#include "esp_ota_ops.h"

namespace Ota {

class AbstractDecoder {
public:
	virtual ~AbstractDecoder() {}
	virtual void init() = 0;
	virtual int decode(const uint8_t *src, size_t len) = 0;
	virtual const uint8_t* data() const = 0;
	virtual size_t len() = 0;
};

class B85decode : public AbstractDecoder{
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

class OtaPacket {
public:
	OtaPacket(char *_d, size_t _l) : m_buf(_d), m_size(_l) {}
	const char* buf() const { return m_buf; }
	size_t len() const { return m_size; }
private:
	char *m_buf;
	size_t m_size;
};

class OtaWorker {
	enum ota_state_e {
		OTA_IDLE, OTA_START, OTA_DATA, OTA_FINISH, OTA_ERROR
	};

public:
	struct md5_update {
		uint8_t md5[16];
		size_t len;
	};

	OtaWorker();
	int handle(const OtaPacket& _p);
	bool isRunning() const { return m_isRunning; }
	void start(const md5_update&);

private:
	void task();
	void otaStart();
	void otaFinish();
	void otaData();

	TimerMember<OtaWorker> m_timeout;
	ota_state_e m_ota_state = OTA_IDLE;
	mbedtls_md5_context m_md5ctx;
	AbstractDecoder *m_decodeCtx;
	md5_update m_md5Updata;
	/* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
	esp_ota_handle_t update_handle = 0;
	const esp_partition_t *update_partition = NULL;
	bool m_isRunning = false;
	size_t sum = 0;
};

}

#endif /* MQTT_USER_OTA_H_ */
