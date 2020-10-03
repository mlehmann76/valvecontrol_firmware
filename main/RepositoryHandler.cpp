/*
 * RepositoryHandler.cpp
 *
 *  Created on: 13.09.2020
 *      Author: marco
 */

#include "RepositoryHandler.h"

#include <regex>
#include "esp_log.h"
#include "repository.h"
#include "utilities.h"
#include "config.h"

#define TAG "RepositoryHandler"

namespace http {

RepositoryHandler::RepositoryHandler(const std::string &_method, const std::string &_path) :
		RequestHandlerBase(_method, _path), m_repositories(){}

bool RepositoryHandler::match(const std::string &_method, const std::string &_path) {
	RequestHandlerBase::regRetType rgx = regMatch(std::regex(path()), _path);
	return rgx.first && hasMethod(_method);
}

bool RepositoryHandler::handle(const HttpRequest& _req, HttpResponse& _res) {
	for (auto &v : m_repositories) {
		if (v.first == _req.path()) {
			_res.setResponse(HttpResponse::HTTP_200);
			_res.setContentType(HttpResponse::CT_APP_JSON);
			_res.send(v.second->stringify());
			_res.reset();
			return true;
		}
	}

	_res.setResponse(HttpResponse::HTTP_404);
	_res.endHeader();
	_res.send(nullptr,0);
	_res.reset();
	return false;
}

} /* namespace http */
