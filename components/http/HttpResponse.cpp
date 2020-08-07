/*
 * HttpResponse.cpp
 *
 *  Created on: 06.08.2020
 *      Author: marco
 */

#include "socket.h"
#include "HttpRequest.h"
#include "HttpResponse.h"

namespace http {

HttpResponse::ResponseMapType HttpResponse::respMap = {
		{HTTP_200, "200 OK"},
		{HTTP_204, "204 No Content"},
		{HTTP_400, "400 Bad Request"},
		{HTTP_401, "401 Unauthorized"},
		{HTTP_404, "404 Not Found"},
		{HTTP_500, "500 Internal Server Error"},
		{HTTP_503, "503 Service Unavailable"},
		{HTTP_511, "511 Network Authentication Required"}
};

HttpResponse::HttpResponse(HttpRequest &_s) : m_respCode(HTTP_500), m_request(&_s){
}

HttpResponse::~HttpResponse() {
}

void HttpResponse::setResponse(ResponseCode _c) {
	m_headerFinished = false;
	m_respCode = _c;
}

void HttpResponse::setHeader(const std::string &s1, const std::string &s2) {
	m_headerEntries.push_back(s1+": "+s2);
}

void HttpResponse::headerAddDate() {
	m_header.append("Date: ");
	m_header.append(getTime());
	m_header.append(LineEnd);
}

void HttpResponse::headerAddServer() {
	m_header.append(std::string("server: valveControl-") + std::string(PROJECT_GIT));
	m_header.append(LineEnd);
}

void HttpResponse::headerAddEntries() {
	for (auto &s : m_headerEntries) {
		m_header.append(s);
		m_header.append(LineEnd);
	}
}

void HttpResponse::addContent(const std::string &_c) {
	if(!m_headerFinished) {
		//TODO add content length
		endHeader();
	}
	m_header.append(_c);
}

const std::string& HttpResponse::get() const{
	return m_header;
}

void HttpResponse::send() {
	m_request->socket()->write(m_header, m_header.size());
}

void HttpResponse::headerAddStatusLine() {
	m_header.append(HTTP_Ver);
	m_header.append(" ");
	m_header.append(respMap[m_respCode]);
	m_header.append(LineEnd);
}

void HttpResponse::endHeader() {
	m_header.clear();
	headerAddStatusLine();
	headerAddEntries();
	headerAddServer();
	headerAddDate();
	m_headerFinished = true;
}

std::string HttpResponse::getTime()
{
    timeval curTime;

    gettimeofday(&curTime, NULL);
    char buf[sizeof "Wed, 21 Oct 2015 07:28:00 GMT"];
    strftime(buf, sizeof buf, "%a, %d %b %Y %T %Z", gmtime(&curTime.tv_sec));
    return buf;
}
} /* namespace http */
