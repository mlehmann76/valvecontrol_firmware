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
#include "iHandler.h"
#include "HttpRequest.h"
#include "HttpResponse.h"

namespace http {

class RequestHandlerBase : public iHandler{
public:
	using regRetType = std::pair<bool,std::smatch>;

public:
	RequestHandlerBase(const std::string &_method, const std::string &_path);
	virtual ~RequestHandlerBase() = default;
	RequestHandlerBase(const RequestHandlerBase &other) = delete;
	RequestHandlerBase(RequestHandlerBase &&other) = delete;
	RequestHandlerBase& operator=(const RequestHandlerBase &other) = delete;
	RequestHandlerBase& operator=(RequestHandlerBase &&other) = delete;

	virtual bool match(const std::string &_method, const std::string &_path);
	virtual bool handle(const HttpRequest &, HttpResponse &) = 0;

	const std::string& path() const {return m_path;}
	const std::string& method() const {return m_method;}

protected:
	regRetType regMatch(const std::regex& rgx, const std::string& s);
	bool hasMethod(const std::string& m);

private:
	static int _pos(const char* s, size_t s_len, const char p);

	std::string m_method;
	std::string m_path;

};

} /* namespace http */

#endif /* COMPONENTS_HTTP_REQUESTHANDLERBASE_H_ */
