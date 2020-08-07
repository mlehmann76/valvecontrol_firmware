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
#include "fmt/printf.h"

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

std::string RequestHandlerBase::_getRandomHexString() {
	return fmt::sprintf("%08x%08x%08x%08x", (uint32_t)rand(), (uint32_t)rand(), (uint32_t)rand(), (uint32_t)rand());
}

void RequestHandlerBase::requestAuth(HTTPAuthMethod mode, const char *realm, const char *failMsg) {
	if (realm == NULL) {
		realm = "Login Required";
	}
	if (mode == BASIC_AUTH) {
		pbrealm = fmt::sprintf("%s%s\"", basicRealm, realm);
	} else {
		nonce = _getRandomHexString();
		opaque = _getRandomHexString();
		pbrealm = fmt::sprintf("%s%s\", qop=\"auth\", nonce=\"%s\", opaque=\"%s\"", digestRealm, realm,
				nonce, opaque);
	}
	if (m_response != nullptr) {
		m_response->setHeader(WWW_Authenticate, pbrealm);
		m_response->setResponse(HttpResponse::HTTP_401);
		m_response->endHeader();
		// End response
	}
}

bool RequestHandlerBase::authenticate(char *buf, size_t buf_len, const char *pUser, const char *pPass) {
	char mode[32];
	char encoded[128];
	char *pdecode;
	bool ret = false;
	size_t outlen = 0;
	sscanf(buf, "%32s %128s", mode, encoded);
	ESP_LOGI(TAG, "Found header => Authorization: mode %s pass %s", mode, encoded);
	//BASIC Mode
	if (0 == ::strncmp(mode, BASIC, sizeof(BASIC))) {
		pdecode = (char*)::base64_decode(encoded, ::strnlen(encoded, sizeof(encoded)), &outlen);
		HttpRequest::ReqPairType _p = HttpRequest::split(std::string(const_cast<char*>(pdecode)));
		if (outlen && pUser != NULL && pPass != NULL) {
			ESP_LOGI(TAG, "Basic Authorization: %s,%s", _p.first.c_str(), _p.second.c_str());
			if (_p.first == std::string(pUser) && _p.second == std::string(pPass)) {
				ESP_LOGI(TAG, "Basic Authorization OK");
				ret = true;
			}
		}
		free(pdecode);
	}
	return ret;
}

} /* namespace http */
