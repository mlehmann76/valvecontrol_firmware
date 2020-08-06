/*
 * PathHandler.h
 *
 *  Created on: 06.08.2020
 *      Author: marco
 */

#ifndef COMPONENTS_HTTP_REQUESTHANDLERBASE_H_
#define COMPONENTS_HTTP_REQUESTHANDLERBASE_H_

#include <string>

#include "HttpRequest.h"
#include "HttpResponse.h"

namespace http {

class RequestHandlerBase {
	static constexpr const char AUTHORIZATION_HEADER[] = "Authorization";
	static constexpr const char WWW_Authenticate[] = "WWW-Authenticate";
	static constexpr const char BASIC[] = "Basic";
	static constexpr const char basicRealm[] = "Basic realm=\"";
	static constexpr const char digestRealm[] = "Digest realm=\"";
public:
	enum HTTPAuthMethod {
		BASIC_AUTH,
		DIGEST_ATH
	} ;

public:
	RequestHandlerBase(const std::string &,const std::string &);
	virtual ~RequestHandlerBase() = default;
	RequestHandlerBase(const RequestHandlerBase &other) = delete;
	RequestHandlerBase(RequestHandlerBase &&other) = delete;
	RequestHandlerBase& operator=(const RequestHandlerBase &other) = delete;
	RequestHandlerBase& operator=(RequestHandlerBase &&other) = delete;

	virtual bool match(const std::string &,const std::string &) = 0;
	virtual void handle(const HttpRequest &, HttpResponse &) = 0;

protected:
	void requestAuth(HTTPAuthMethod mode, const char *realm, const char *failMsg);
	bool authenticate(char *buf, size_t buf_len, const char* pUser, const char* pPass);

private:
	static int _pos(const char* s, size_t s_len, const char p);
	static void _getRandomHexString(char *buf, size_t len);

	HttpResponse *m_response;
	std::string m_method;
	std::string m_path;
	char nonce[33];
	char opaque[33];
	char pbrealm[256];
};

} /* namespace http */

#endif /* COMPONENTS_HTTP_REQUESTHANDLERBASE_H_ */
