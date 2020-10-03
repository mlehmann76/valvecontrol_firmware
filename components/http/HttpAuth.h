/*
 * AuthProxy.h
 *
 *  Created on: 16.09.2020
 *      Author: marco
 */

#ifndef COMPONENTS_HTTP_HTTPAUTH_H_
#define COMPONENTS_HTTP_HTTPAUTH_H_

#include "iHandler.h"
#include <string>

namespace http {

class HttpAuth: public iHandler {
	static constexpr const char AUTHORIZATION_HEADER[] = "Authorization";
	static constexpr const char WWW_Authenticate[] = "WWW-Authenticate";
	static constexpr const char BASIC[] = "Basic";
	static constexpr const char DIGEST[] = "Digest";
	static constexpr const char basicRealm[] = "Basic realm=\"";
	static constexpr const char digestRealm[] = "Digest realm=\"";

public:

	struct AuthToken {
		std::string user;
		std::string pass;
	};

	enum HTTPAuthMethod {
		BASIC_AUTH,
		DIGEST_AUTH_MD5,
		DIGEST_AUTH_SHA256,
		DIGEST_AUTH_SHA256_MD5
	} ;

	virtual ~HttpAuth() = default;
	HttpAuth(iHandler* _h, AuthToken _at, HTTPAuthMethod mode = BASIC_AUTH) : m_handler(_h), m_response(), m_request(), m_mode(mode), m_token(std::move(_at)) {}
	HttpAuth(const HttpAuth &other) = delete;
	HttpAuth(HttpAuth &&other) = delete;
	HttpAuth& operator=(const HttpAuth &other) = default;
	HttpAuth& operator=(HttpAuth &&other) = default;
	//iHandler
	bool match(const std::string &_method, const std::string &_path) override;
	bool handle(const HttpRequest &, HttpResponse &) override;
protected:
	void requestAuth(HTTPAuthMethod mode, const char *realm, const char *failMsg);
	bool authenticate(const std::string& ans, const AuthToken& _t);

private:
	static std::string _getRandomHexString();
	static std::string ToHex(const unsigned char *s, size_t len);
	static std::string ToMD5HexStr(const std::string& s);
	static std::string ToSHA256HexStr(const std::string& s);
	void genrealm(const char *realm, const char *method);

	iHandler* m_handler;
	HttpResponse *m_response;
	const HttpRequest *m_request;
	HTTPAuthMethod m_mode;
	std::string nonce;
	std::string opaque;
	std::string pbrealm;
	AuthToken m_token;
};

} /* namespace http */

#endif /* COMPONENTS_HTTP_HTTPAUTH_H_ */
