/*
 * PathHandler.cpp
 *
 *  Created on: 06.08.2020
 *      Author: marco
 */

#include <string>
#include <cstring>
#include "config_user.h"
#include "esp_log.h"
#include "RequestHandlerBase.h"

extern "C" {
#include "../esp-idf/components/wpa_supplicant/src/utils/base64.h"
}

#define TAG "RESPONSE"

namespace http {

RequestHandlerBase::RequestHandlerBase(const std::string &_method, const std::string &_path) :
		m_response(nullptr), m_method(_method), m_path(_path) {
}

int RequestHandlerBase::_pos(const char *s, size_t s_len, const char p) {
	int ret = -1;
	for (int i = 0; i < s_len; i++) {
		if (s[i] == p) {
			ret = i;
			break;
		}
	}
	return ret;
}

void RequestHandlerBase::_getRandomHexString(char *buf, size_t len) {
	for (int i = 0; i < 4; i++) {
		snprintf(buf + (i * 8), len, "%08x", (uint32_t)rand());
		len -= 8;
	}
}

void RequestHandlerBase::requestAuth(HTTPAuthMethod mode, const char *realm, const char *failMsg) {
	if (realm == NULL) {
		realm = "Login Required";
	}
	if (mode == BASIC_AUTH) {
		snprintf(pbrealm, sizeof(pbrealm), "%s%s\"", basicRealm, realm);
	} else {
		_getRandomHexString(nonce, sizeof(nonce));
		_getRandomHexString(opaque, sizeof(opaque));
		snprintf(pbrealm, sizeof(pbrealm), "%s%s\", qop=\"auth\", nonce=\"%s\", opaque=\"%s\"", digestRealm, realm,
				nonce, opaque);
	}
	if (m_response != nullptr) {
		m_response->setHeader(WWW_Authenticate, pbrealm);
		m_response->setResponse(HttpResponse::HTTP_401);
		m_response->endHeader();
		// End response
		m_response->send("");
	}
}

bool RequestHandlerBase::authenticate(char *buf, size_t buf_len, const char *pUser, const char *pPass) {
	char mode[32];
	char encoded[128];
	unsigned char *pdecode;
	bool ret = false;
	size_t outlen = 0;
	sscanf(buf, "%32s %128s", mode, encoded);
	ESP_LOGI(TAG, "Found header => Authorization: mode %s pass %s", mode, encoded);
	//BASIC Mode
	if (0 == ::strncmp(mode, BASIC, sizeof(BASIC))) {
		pdecode = ::base64_decode(encoded, ::strnlen(encoded, sizeof(encoded)), &outlen);
		if (outlen && pUser != NULL && pPass != NULL) {
			char user[128];
			char passw[128];
			int posc = _pos((char*) pdecode, outlen, ':');
			if (posc != -1) {
				::memcpy(user, pdecode, posc);
				user[posc] = 0;
				::memcpy(passw, &pdecode[posc + 1], outlen - posc);
				passw[outlen - posc] = 0;
			}
			ESP_LOGI(TAG, "Basic Authorization: %s,%s", user, passw);
			if ((0 == ::strncmp(user, pUser, std::max(strlen(user), strlen(pUser))))
					&& (0 == ::strncmp(passw, pPass, std::max(strlen(passw), strlen(pPass))))) {
				ESP_LOGI(TAG, "Basic Authorization OK");
				ret = true;
			}
		}
		free(pdecode);
	}
	return ret;
}

} /* namespace http */
