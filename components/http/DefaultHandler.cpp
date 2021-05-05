/*
 * DefaultHandler.cpp
 *
 *  Created on: 07.08.2020
 *      Author: marco
 */

#include "DefaultHandler.h"
#include "socket.h"

namespace http {

DefaultHandler::DefaultHandler() : RequestHandlerBase("", "") {}

DefaultHandler::~DefaultHandler() {}

bool DefaultHandler::match(const std::string &method, const std::string &path) {
    return true; // match all if none before has matched
}

bool DefaultHandler::handle(const HttpRequest &_req, HttpResponse &_res) {
    if (_req.method() == "OPTIONS") {
        _res.setResponse(HttpResponse::HTTP_204);
        _res.setHeader("Allow", "OPTIONS, GET, HEAD, POST, PUT, DELETE");
        _res.setHeaderDefaults();
        _res.endHeader();
    } else {
        _res.setResponse(HttpResponse::HTTP_404);
        _res.setHeaderDefaults();
        _res.endHeader();
    }
    _res.send("");
    return false;
}

} /* namespace http */
