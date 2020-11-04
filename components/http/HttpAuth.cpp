/*
 * AuthProxy.cpp
 *
 *  Created on: 16.09.2020
 *      Author: marco
 */

#include "HttpAuth.h"

#include <cstring>
#include <fmt/printf.h>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <regex>
#include <sstream>
#include <string>

#include "Base64.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "mbedtls/md5.h"
#include "mbedtls/sha256.h"
#include "utilities.h"

#define TAG "AuthProxy"

namespace http {

constexpr const char HttpAuth::AUTHORIZATION_HEADER[];
constexpr const char HttpAuth::WWW_Authenticate[];
constexpr const char HttpAuth::BASIC[];
constexpr const char HttpAuth::DIGEST[];
constexpr const char HttpAuth::basicRealm[];
constexpr const char HttpAuth::digestRealm[];

bool HttpAuth::match(const std::string &_method, const std::string &_path) {
    return m_handler->match(_method, _path);
}

bool HttpAuth::handle(const HttpRequest &_req, HttpResponse &_res) {
    m_response = &_res;
    m_request = &_req;
    auto it = _req.header().find(AUTHORIZATION_HEADER);
    if (it != _req.header().end() && authenticate(it->second, m_token)) {
        return m_handler->handle(_req, _res);
    }
    requestAuth(m_mode, nullptr, nullptr);
    return false;
}

std::string HttpAuth::_getRandomHexString() {
    return fmt::sprintf("%08x%08x%08x%08x", (uint32_t)rand(), (uint32_t)rand(),
                        (uint32_t)rand(), (uint32_t)rand());
}

std::string HttpAuth::ToHex(const unsigned char *s, size_t len) {
    std::ostringstream ret;
    for (std::string::size_type i = 0; i < len; ++i) {
        ret << std::hex << (s[i] / 16) << (s[i] % 16);
    }
    return ret.str();
}

std::string HttpAuth::ToMD5HexStr(const std::string &s) {
    unsigned char ha1[16];
    mbedtls_md5((unsigned char *)s.data(), s.length(), ha1);
    return ToHex(ha1, 16);
}

std::string HttpAuth::ToSHA256HexStr(const std::string &s) {
    unsigned char ha1[32];
    mbedtls_sha256((unsigned char *)s.data(), s.length(), ha1, 0);
    return ToHex(ha1, 32);
}

void HttpAuth::genrealm(const char *realm, const char *method) {
    pbrealm = fmt::sprintf(
        "%s%s\", qop=\"auth\", algorithm=\"%s\", nonce=\"%s\", opaque=\"%s\"",
        digestRealm, realm, method, nonce.c_str(), opaque.c_str());
}

void HttpAuth::requestAuth(HTTPAuthMethod mode, const char *realm,
                           const char *failMsg) {
    if (m_response != nullptr) {
        if (realm == NULL) {
            realm = "Login Required";
        }
        if (mode != BASIC_AUTH) {
            nonce = _getRandomHexString();
            opaque = _getRandomHexString();
        }

        switch (mode) {
        case BASIC_AUTH:
            pbrealm = fmt::sprintf("%s%s\"", basicRealm, realm);
            break;
        case DIGEST_AUTH_MD5:
            genrealm(realm, "MD5");
            break;
        case DIGEST_AUTH_SHA256:
            genrealm(realm, "SHA-256");
            break;
        case DIGEST_AUTH_SHA256_MD5:
            genrealm(realm, "SHA-256");
            m_response->setHeader(WWW_Authenticate, pbrealm);
            genrealm(realm, "MD5");
            break;
        }
        m_response->setHeader(WWW_Authenticate, pbrealm);
        m_response->setResponse(HttpResponse::HTTP_401);
        m_response->endHeader();
        m_response->send(nullptr, 0);
        m_response->reset();
    }
}

bool HttpAuth::authenticate(const std::string &ans, const AuthToken &_t) {
    bool ret = false;
    std::vector<std::string> _b = HttpRequest::split(ans, " "); // FIXME
    // ESP_LOGV(TAG, "Authorization: %s", ans.c_str());
    // BASIC Mode
    if (_b[0] == BASIC) {
        std::string decode;
        macaron::Base64::Decode(_b[1], decode);
        if (decode.length()) {
            std::vector<std::string> _p = HttpRequest::split(decode, ":");
            if (_p[0] == _t.user && _p[1] == _t.pass) {
                // ESP_LOGV(TAG, "Basic Authorization OK");
                ret = true;
            }
        }
    } else if ((m_request != nullptr) && _b[0] == DIGEST) {

        const std::string auth = ": Digest";
        size_t pos = ans.find(auth, 0);
        const std::string tx = ans.substr(pos + auth.length());

        std::vector<std::string> tokens = HttpRequest::split(tx, ", ");
        std::map<std::string, std::string> tokenmap;

        for (auto &v : tokens) {
            std::vector<std::string> token = HttpRequest::split(v, "=");
            tokenmap[token[0]] =
                std::regex_replace(token[1], std::regex("\\\""), "");
        }
        std::function<std::string(const std::string &)> _hashFunc =
            HttpAuth::ToMD5HexStr;

        if (tokenmap["algorithm"] == "SHA-256") {
            _hashFunc = HttpAuth::ToSHA256HexStr;
        }

        // HA1 = MD5(username:realm:password)
        std::string sha1 = _hashFunc(tokenmap["username"] + ":" +
                                     tokenmap["realm"] + ":" + _t.pass);
        // HA2 = MD5(method:digestURI)
        std::string sha2 =
            _hashFunc(m_request->method() + ":" + tokenmap["uri"]);
        // If the qop directive's value is "auth" or "auth-int", then compute
        // the response as follows: response =
        // MD5(HA1:nonce:nonceCount:cnonce:qop:HA2)
        std::string sres = _hashFunc(sha1 + ":" + tokenmap["nonce"] + ":" +
                                     tokenmap["nc"] + ":" + tokenmap["cnonce"] +
                                     ":" + tokenmap["qop"] + ":" + sha2);

        ret = (sres == tokenmap["response"]);
    }
    return ret;
}

} /* namespace http */
