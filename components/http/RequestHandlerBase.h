/*
 * PathHandler.h
 *
 *  Created on: 06.08.2020
 *      Author: marco
 */

#ifndef COMPONENTS_HTTP_REQUESTHANDLERBASE_H_
#define COMPONENTS_HTTP_REQUESTHANDLERBASE_H_

#include <string>
#include <regex>
#include <tuple>

#include "HttpRequest.h"
#include "HttpResponse.h"

namespace http {

class RequestHandlerBase {
public:
	using regRetType = std::pair<bool,std::smatch>;
	static constexpr const char AUTHORIZATION_HEADER[] = "Authorization";
	static constexpr const char WWW_Authenticate[] = "WWW-Authenticate";
	static constexpr const char BASIC[] = "Basic";
	static constexpr const char basicRealm[] = "Basic realm=\"";
	static constexpr const char digestRealm[] = "Digest realm=\"";
	enum HTTPAuthMethod {
		BASIC_AUTH,
		DIGEST_ATH
	} ;

public:
	RequestHandlerBase(const std::string &_method, const std::string &_path);
	virtual ~RequestHandlerBase() = default;
	RequestHandlerBase(const RequestHandlerBase &other) = delete;
	RequestHandlerBase(RequestHandlerBase &&other) = delete;
	RequestHandlerBase& operator=(const RequestHandlerBase &other) = delete;
	RequestHandlerBase& operator=(RequestHandlerBase &&other) = delete;

	virtual bool match(const std::string &_method, const std::string &_path) = 0;
	virtual bool handle(const HttpRequest &, HttpResponse &) = 0;

	const std::string& path() const {return m_path;}
	const std::string& method() const {return m_method;}

protected:
	void setResponse(HttpResponse &resp) {m_response = &resp;}
	HttpResponse *getResponse() {return m_response; }
	void requestAuth(HTTPAuthMethod mode, const char *realm, const char *failMsg);
	bool authenticate(const char *buf, size_t buf_len, const std::string &pUser, const std::string &pPass);
	regRetType regMatch(const std::regex& rgx, const std::string& s);
	bool hasMethod(const std::string& m);

private:
	static int _pos(const char* s, size_t s_len, const char p);
	static std::string _getRandomHexString();

	HttpResponse *m_response;
	std::string m_method;
	std::string m_path;
	std::string nonce;
	std::string opaque;
	std::string pbrealm;
};

} /* namespace http */

#endif /* COMPONENTS_HTTP_REQUESTHANDLERBASE_H_ */
