/*
 * HttpResponse.cpp
 *
 *  Created on: 06.08.2020
 *      Author: marco
 */

#include "HttpResponse.h"

namespace http {

HttpResponse::HttpResponse() : m_respCode(HTTP_500){
}

HttpResponse::~HttpResponse() {
}

void HttpResponse::setResponse(ResponseCode _c) {
	m_respCode = _c;
}

void HttpResponse::setHeader(const std::string &s1, const std::string &s2) {
	m_headerEntries.push_back(s1+": "+s2);
}

void HttpResponse::endHeader() {
	m_header.clear();
	m_header.append(HTTP_Ver);
	m_header.append(" ");
	m_header.append(respMap.at(m_respCode));
	m_header.append(LineEnd);
	for(auto &s : m_headerEntries) {
		m_header.append(s);
		m_header.append(LineEnd);
	}
	m_header.append(LineEnd);
}

void HttpResponse::send(const std::string &allocator) {
}

} /* namespace http */
