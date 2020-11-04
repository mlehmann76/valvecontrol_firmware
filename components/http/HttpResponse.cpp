/*
 * HttpResponse.cpp
 *
 *  Created on: 06.08.2020
 *      Author: marco
 */

#include "HttpResponse.h"
#include "HttpRequest.h"
#include <fmt/printf.h>

#include "socket.h"
#include "utilities.h"

#include <sys/time.h>

namespace http {
//@formatter:off
HttpResponse::ResponseMapType HttpResponse::s_respMap = {
    {HTTP_200, "200 OK"},
    {HTTP_204, "204 No Content"},
    {HTTP_400, "400 Bad Request"},
    {HTTP_401, "401 Unauthorized"},
    {HTTP_404, "404 Not Found"},
    {HTTP_500, "500 Internal Server Error"},
    {HTTP_503, "503 Service Unavailable"},
    {HTTP_511, "511 Network Authentication Required"}};

HttpResponse::ContentTypeMapType HttpResponse::s_ctMap = {
    {CT_APP_JSON, "application/json"},
    {CT_APP_OCSTREAM, "application/octet-stream"},
    {CT_APP_PDF, "application/pdf"},
    {CT_TEXT_JAVASCRIPT, "text/javascript"},
    {CT_TEXT_HTML, "text/html"},
    {CT_TEXT_PLAIN, "text/plain"},
};

HttpResponse::FileContentMapType HttpResponse::s_fcmap = {
    {".html", CT_TEXT_HTML},
    {".htm", CT_TEXT_HTML},
    {".js", CT_TEXT_JAVASCRIPT},
    {".pdf", CT_APP_PDF},
};

//@formatter:on
HttpResponse::HttpResponse(HttpRequest &_s)
    : m_respCode(HTTP_500), m_request(&_s), m_headerFinished(false),
      m_firstChunkSent(false) {}

HttpResponse::~HttpResponse() {}

void HttpResponse::setResponse(ResponseCode _c) {
    m_headerFinished = false;
    m_respCode = _c;
}

void HttpResponse::setHeader(const std::string &s1, const std::string &s2) {
    m_headerEntries.push_back(s1 + ": " + s2);
}

void HttpResponse::headerAddDate() {
    m_header.append("Date: ");
    m_header.append(getTime());
    m_header.append(LineEnd);
}

void HttpResponse::headerAddServer() {
    m_header.append("server: valveControl-");
    // m_header.append(PROJECT_GIT);
    m_header.append(LineEnd);
}

void HttpResponse::headerAddEntries() {
    for (auto &s : m_headerEntries) {
        m_header.append(s);
        m_header.append(LineEnd);
    }
}

void HttpResponse::setContentType(ContentType _c) {
    m_headerEntries.push_back(fmt::sprintf("Content-Type: %s", s_ctMap[_c]));
}

void HttpResponse::headerAddStatusLine() {
    m_header.append(HTTP_Ver);
    m_header.append(" ");
    m_header.append(s_respMap[m_respCode]);
    m_header.append(LineEnd);
}

void HttpResponse::endHeader() {
    m_header.clear();
    headerAddStatusLine();
    headerAddEntries();
    headerAddServer();
    headerAddDate();
    m_header.append(LineEnd);

    m_headerFinished = true;
}

void HttpResponse::send(const std::string &_a) { send(_a.data(), _a.length()); }

void HttpResponse::send_chunk(const std::string &_chunk) {
    send_chunk(_chunk.data(), _chunk.length());
}

void HttpResponse::send(const char *_buf, size_t _s) {
    if (!m_headerFinished) {
        if (_buf != nullptr) {
            m_headerEntries.push_back(fmt::sprintf("Content-Length: %d", _s));
        }
        endHeader();
    }
    m_request->socket()->write(m_header, m_header.length());

    if (_buf != nullptr) {
        m_request->socket()->write(_buf, _s);
    }
}

void HttpResponse::send_chunk(const char *_buf, size_t _s) {
    if (!m_firstChunkSent) {
        m_headerEntries.push_back("Transfer-Encoding: chunked");
        endHeader();
        m_request->socket()->write(m_header, m_header.length());
        m_firstChunkSent = true;
    } // else {
    if (_buf != nullptr) {
        std::string _chunkLen = fmt::sprintf("%x\r\n", _s);
        m_request->socket()->write(_chunkLen, _chunkLen.length());
        m_request->socket()->write(_buf, _s);
        m_request->socket()->write(LineEnd, strlen(LineEnd));
    }
    //}
}

std::string HttpResponse::getTime() {
    timeval curTime;
    gettimeofday(&curTime, NULL);
    char buf[sizeof "Wed, 21 Oct 2015 07:28:00 GMT"];
    strftime(buf, sizeof buf, "%a, %d %b %Y %T %Z", gmtime(&curTime.tv_sec));
    return buf;
}

void HttpResponse::reset() {
    m_respCode = HTTP_500;
    m_header.clear();
    m_headerEntries.clear();
    m_headerFinished = false;
}

HttpResponse::ContentType
HttpResponse::nameToContentType(const std::string &_name) {
    std::string::size_type _end = _name.find_last_of('.');
    return _end != std::string::npos ? s_fcmap[_name.substr(_end, 5)]
                                     : CT_TEXT_PLAIN;
}

} /* namespace http */
