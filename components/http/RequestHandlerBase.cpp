/*
 * PathHandler.cpp
 *
 *  Created on: 06.08.2020
 *      Author: marco
 */

#include "RequestHandlerBase.h"
#include "utilities.h"
#include <cstring>
#include <string>

#define TAG "RequestHandlerBase"

namespace http {

RequestHandlerBase::RequestHandlerBase(const std::string &_method,
                                       const std::string &_path)
    : m_method(_method), m_path(_path) {}

bool RequestHandlerBase::match(const std::string &_method,
                               const std::string &_path) {
    regRetType rgx = regMatch(std::regex(path()), _path);
    return rgx.first && hasMethod(_method);
}

int RequestHandlerBase::_pos(const char *s, size_t s_len, const char p) {
    int ret = -1;
    for (int i = 0; i < s_len; i++) {
        if (s[i] == p) {
            ret = i;
            break;
        }
    }
    return ret;
}

RequestHandlerBase::regRetType
RequestHandlerBase::regMatch(const std::regex &rgx, const std::string &s) {
    std::smatch matches;
    bool ret = std::regex_search(s, matches, rgx);
    return std::make_pair(ret, matches);
}

bool RequestHandlerBase::hasMethod(const std::string &_method) {
    bool ret = false;
    auto split = utilities::split(method(), ",");
    for (const auto &s : split) {
        if (s == _method) {
            ret = true;
            break;
        }
    }
    return ret;
}

} /* namespace http */
