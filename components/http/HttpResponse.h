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
#include <unordered_map>
#include "frozen/unordered_map.h"
#include "frozen/string.h"


namespace http {

class HttpRequest;

class HttpResponse {
	static constexpr const char *HTTP_Ver = "HTTP/1.1";
	static constexpr const char *LineEnd = "\r\n";

public:
	enum ResponseCode {
		HTTP_200, HTTP_204, HTTP_400, HTTP_401, HTTP_404, HTTP_500, HTTP_503, HTTP_511
	};

	using ResponseMapType = std::unordered_map<ResponseCode, const char*>;

	static ResponseMapType respMap;

	HttpResponse(HttpRequest &_s);
	virtual ~HttpResponse();
	HttpResponse(const HttpResponse &other) = delete;
	HttpResponse(HttpResponse &&other) = delete;
	HttpResponse& operator=(const HttpResponse &other) = delete;
	HttpResponse& operator=(HttpResponse &&other) = delete;

	void setResponse(ResponseCode _c);
	void setHeader(const std::string &, const std::string &);
	void endHeader();

	void addContent(const std::string &);
	void send();
	const std::string& get() const;

private:

	std::string getTime();
	void headerAddDate();
	void headerAddServer();
	void headerAddEntries();
	void headerAddStatusLine();

	ResponseCode m_respCode;
	std::string m_header;
	std::vector<std::string> m_headerEntries;
	HttpRequest *m_request;
	bool m_headerFinished = false;
};

} /* namespace http */

#endif /* COMPONENTS_HTTP_HTTPRESPONSE_H_ */
