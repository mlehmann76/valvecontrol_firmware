/*
 * HttpResponse.h
 *
 *  Created on: 06.08.2020
 *      Author: marco
 */

#ifndef COMPONENTS_HTTP_HTTPRESPONSE_H_
#define COMPONENTS_HTTP_HTTPRESPONSE_H_

#include <string>
#include <vector>
#include "frozen/unordered_map.h"
#include "frozen/string.h"

namespace http {

class HttpResponse {
	static constexpr const char *HTTP_Ver = "HTTP/1.1";
	static constexpr const char *LineEnd = "\r\n";

public:
	enum ResponseCode {
		HTTP_200, HTTP_204, HTTP_400, HTTP_401, HTTP_404, HTTP_500, HTTP_503, HTTP_511
	};

	using ResponseMapType = frozen::unordered_map<ResponseCode, const char*, 8>;

	static constexpr ResponseMapType respMap = {
			{HTTP_200, "200 OK"},
			{HTTP_204, "No Content"},
			{HTTP_400, "Bad Request"},
			{HTTP_401, "Unauthorized"},
			{HTTP_404, "Not Found"},
			{HTTP_500, "Internal Server Error"},
			{HTTP_503, "Service Unavailable"},
			{HTTP_511, "Network Authentication Required"}
	};

	HttpResponse();
	virtual ~HttpResponse();
	HttpResponse(const HttpResponse &other) = delete;
	HttpResponse(HttpResponse &&other) = delete;
	HttpResponse& operator=(const HttpResponse &other) = delete;
	HttpResponse& operator=(HttpResponse &&other) = delete;

	void setResponse(ResponseCode _c);
	void setHeader(const std::string &, const std::string &);
	void endHeader();

	void send(const std::string &);

private:
	ResponseCode m_respCode;
	std::string m_header;
	std::vector<std::string> m_headerEntries;
};

} /* namespace http */

#endif /* COMPONENTS_HTTP_HTTPRESPONSE_H_ */
