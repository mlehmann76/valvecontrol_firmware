/*
 * HttpRequest.h
 *
 *  Created on: 05.08.2020
 *      Author: marco
 */

#ifndef MAIN_HTTPREQUEST_H_
#define MAIN_HTTPREQUEST_H_

#include <string>
#include <vector>
#include <unordered_map>

class HttpRequest {
public:
	enum Method {
		NONE, GET, HEAD, POST, PUT, DELETE, CONNECT, OPTIONS, TRACE
	};
	using Header = std::vector<std::pair<std::string,std::string>>;
	using MethodMapType = std::unordered_map<const char*, enum Method>;

private:
	static MethodMapType MethodMap;
	static constexpr const char *LineEnd = "\r\n";

public:
	HttpRequest(const std::string&);
	virtual ~HttpRequest();
	HttpRequest(const HttpRequest &other) = delete;
	HttpRequest(HttpRequest &&other) = delete;
	HttpRequest& operator=(const HttpRequest &other) = delete;
	HttpRequest& operator=(HttpRequest &&other) = delete;

	Method method() {
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

private:
	std::pair<std::string, std::string> split(const std::string &line);
	std::vector<std::string> split(const std::string &s, const std::string &seperator);

	void analyze(const std::string&);

	std::string m_request;
	Method m_method;
	std::string m_path;
	std::string m_version;
	Header m_header;
};

#endif /* MAIN_HTTPREQUEST_H_ */
