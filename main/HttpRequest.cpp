/*
 * HttpRequest.cpp
 *
 *  Created on: 05.08.2020
 *      Author: marco
 */

#include <esp_log.h>
#include "config_user.h"
#include "HttpRequest.h"

#define TAG "HTTPREQUEST"

HttpRequest::MethodMapType HttpRequest::MethodMap = //
		{ { "", NONE }, { "GET", GET }, //
				{ "HEAD", HEAD }, { "POST", POST }, //
				{ "PUT", PUT }, { "DELETE", DELETE }, //
				{ "CONNECT", CONNECT }, { "OPTIONS", OPTIONS }, //
				{ "TRACE", TRACE } };

HttpRequest::HttpRequest(const std::string &_r) :
		m_request(_r), m_method(NONE) {
	analyze(_r);
}

HttpRequest::~HttpRequest() {
}

std::pair<std::string, std::string> HttpRequest::split(const std::string &line) {
	std::pair<std::string, std::string> token;
	std::size_t start = 0, end = 0;
	if ((end = line.find(":", start)) != std::string::npos) {
		token = { line.substr(0, end), line.substr(end, line.length()) };
	}
	return token;
}

//https://stackoverflow.com/questions/236129/how-do-i-iterate-over-the-words-of-a-string
std::vector<std::string> HttpRequest::split(const std::string &text, const std::string &delims) {
	std::vector<std::string> tokens;
	std::size_t start = text.find_first_not_of(delims), end = 0;

	while ((end = text.find_first_of(delims, start)) != std::string::npos) {
		tokens.push_back(text.substr(start, end - start));
		start = text.find_first_not_of(delims, end);
	}
	if (start != std::string::npos)
		tokens.push_back(text.substr(start));

	return tokens;
}

void HttpRequest::analyze(const std::string &r) {
	std::vector<std::string> lines = split(r, LineEnd);
	if (lines.size()) {
		ESP_LOGV(TAG, "request first line %s", lines[0].c_str());
		std::vector<std::string> first = split(lines[0], " ");
		if (first.size() == 3) {
			//test Method
			m_method = MethodMap[first[0].c_str()];
			m_path = first[1];
			m_version = first[2];
		}
	}
	for (size_t i = 1; i < lines.size(); i++) {
		m_header.push_back(split(lines[i]));
	}
}
