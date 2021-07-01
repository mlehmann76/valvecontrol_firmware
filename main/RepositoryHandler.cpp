/*
 * RepositoryHandler.cpp
 *
 *  Created on: 13.09.2020
 *      Author: marco
 */

#include "RepositoryHandler.h"

#include "config_user.h"
#include "logger.h"
#include "repository.h"
#include "utilities.h"
#include <regex>

#define TAG "RepositoryHandler"

namespace http {

RepositoryHandler::RepositoryHandler(const std::string &_method,
                                     const std::string &_path)
    : RequestHandlerBase(_method, _path), m_repositories() {}

bool RepositoryHandler::handle(const HttpRequest &_req, HttpResponse &_res) {
    // log_inst.debug(TAG, "{},  {}", _req.method(), _req.path());
	_res.reset();
    for (auto &v : m_repositories) {
        const std::string attr =
            std::string(_req.path()).erase(0, v.first.length());
        const std::string path =
            std::string(_req.path()).erase(v.first.length());

        log_inst.debug(TAG, "attr : %s , path : %s", attr.c_str(),
                       path.c_str());
        if (v.first == path) {
            if (_req.hasMethod(HttpRequest::GET)) {
                _res.setResponse(HttpResponse::HTTP_200);
                _res.setContentType(HttpResponse::CT_APP_JSON);
                _res.setHeaderDefaults();
                _res.send(v.second->stringify(v.second->partial(attr)));
                return true;
            } else if (_req.hasMethod(HttpRequest::POST)) {
                _res.setResponse(HttpResponse::HTTP_201);
                _res.setHeaderDefaults();
                _res.setHeader("Content-Location", _req.path());
                _res.send("");
                v.second->append(_req.body());
                return true;
            } else if (_req.hasMethod(HttpRequest::DELETE)) {
                _res.setResponse(HttpResponse::HTTP_201);
                _res.setHeaderDefaults();
                _res.setHeader("Content-Location", _req.path());
                _res.send("");
                v.second->remove(_req.body());
                return true;
            }
        }
    }

    _res.setResponse(HttpResponse::HTTP_404);
    _res.endHeader();
    _res.send(nullptr, 0);
    return false;
}

} /* namespace http */
