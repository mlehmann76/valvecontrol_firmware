/*
 * HttpRequest.h
 *
 *  Created on: 05.08.2020
 *      Author: marco
 */

#ifndef COMPONENTS_HTTP_HTTPREQUEST_H_
#define COMPONENTS_HTTP_HTTPREQUEST_H_

#include <string>
#include <vector>
#include <unordered_map>

namespace http {

class HttpRequest {
public:
	enum Method {
		NONE, GET, HEAD, POST, PUT, DELETE, CONNECT, OPTIONS, TRACE
	};
	using Header = std::unordered_map<std::string,std::string>;
	using MethodMapType = std::unordered_map<const char*, enum Method>;
	using ReqPairType = std::pair<std::string, std::string>;

private:
	static MethodMapType MethodMap;
	static constexpr const char *LineEnd = "\r\n";

public:
	HttpRequest();
	HttpRequest(const std::string&);
	virtual ~HttpRequest();
	HttpRequest(const HttpRequest &other) = delete;
	HttpRequest(HttpRequest &&other) = delete;
	HttpRequest& operator=(const HttpRequest &other) = delete;
	HttpRequest& operator=(HttpRequest &&other) = delete;

	const std::string& method() const{
		return m_method;
	}
	const std::string& path() const {
		return m_path;
	}
	const std::string& version() const {
		return m_version;
	}
	const Header& header() const {
		return m_header;
	}

	static ReqPairType split(const std::string &line);
	static std::vector<std::string> split(const std::string &s, const std::string &seperator);

	void analyze(const std::string&);

private:

	std::string m_method;
	std::string m_path;
	std::string m_version;
	Header m_header;
};

} /* namespace http */
#endif /* COMPONENTS_HTTP_HTTPREQUEST_H_ */
