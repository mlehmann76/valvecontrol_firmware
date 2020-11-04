/*
 * HttpRequest.cpp
 *
 *  Created on: 05.08.2020
 *      Author: marco
 */

#include "HttpRequest.h"
#include "socket.h"

#include <iostream>

#define TAG "HTTPREQUEST"

namespace http {

HttpRequest::MethodMapType HttpRequest::MethodMap = //
    {{"", NONE},           {"GET", GET},            //
     {"HEAD", HEAD},       {"POST", POST},          //
     {"PUT", PUT},         {"DELETE", DELETE},      //
     {"CONNECT", CONNECT}, {"OPTIONS", OPTIONS},    //
     {"TRACE", TRACE}};

HttpRequest::ParseResult HttpRequest::parse() {
    ParseResult ret = PARSE_NODATA;
    if (m_socket != nullptr) {
        std::string _buf;
        switch (m_socket->pollConnectionState(std::chrono::milliseconds(10))) {
        case Socket::noData:
            ret = PARSE_NODATA;
            break;
        case Socket::errorState:
            ret = PARSE_ERROR;
            m_socket->close();
            break;
        case Socket::newData:
            int readSize = m_socket->read(_buf, 1024);
            if (readSize > 0) {
                // TODO just forward if part of a bigger message
                analyze(_buf);
                ret = PARSE_OK;
            } else if (readSize == -1) {
                ret = PARSE_ERROR;
                m_socket->close();
            }
            break;
        }
    }
    return ret;
}

HttpRequest::HttpRequest(Socket *_s) : m_socket(_s) {}

HttpRequest::~HttpRequest() {}

std::pair<std::string, std::string>
HttpRequest::splitHeaderLine(const std::string &line) {
    std::pair<std::string, std::string> token;
    std::vector<std::string> _l = split(line, ": ");
    if (_l.size() > 1) {
        token = {_l[0], _l[1]};
    }
    return token;
}

std::vector<std::string> HttpRequest::split(const std::string &text,
                                            const std::string &delims) {
    std::vector<std::string> tokens;
    std::size_t start = 0, end = 0;

    while ((end = text.find(delims, start)) != std::string::npos) {
        tokens.push_back(text.substr(start, end - start));
        start = end + delims.length();
    }
    if (start != std::string::npos)
        tokens.push_back(text.substr(start));

    return tokens;
}

void HttpRequest::analyze(const std::string &r) {
    m_header.clear();
    std::vector<std::string> lines = split(r, LineEnd);
    if (lines.size()) {
        std::vector<std::string> first = split(lines[0], " ");
        if (first.size() == 3) {
            // TODO check if Method valid
            m_method = first[0];
            m_path = first[1];
            m_version = first[2];
        }
    }
    size_t i;
    for (i = 1; lines[i].length() > 0 && lines[i] != LineEnd; i++) {
        ReqPairType p = splitHeaderLine(lines[i]);
        m_header[p.first] = p.second;
    }
    // skip empty line
    ++i;
    // check for body
    if (i < lines.size()) {
        m_body = lines[i];
    }
}

} /* namespace http */
