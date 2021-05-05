/*
 * PathHandler.h
 *
 *  Created on: 06.08.2020
 *      Author: marco
 */

#ifndef COMPONENTS_HTTP_REQUESTHANDLERBASE_H_
#define COMPONENTS_HTTP_REQUESTHANDLERBASE_H_

#include "HttpRequest.h"
#include "HttpResponse.h"
#include "iHandler.h"
#include <regex>
#include <string>
#include <tuple>

namespace http {

class RequestHandlerBase : public iHandler {
  public:
    using regRetType = std::pair<bool, std::smatch>;

  public:
    RequestHandlerBase(const std::string &_method, const std::string &_path);
    virtual ~RequestHandlerBase() = default;
    RequestHandlerBase(const RequestHandlerBase &other) = default;
    RequestHandlerBase(RequestHandlerBase &&other) = default;
    RequestHandlerBase &operator=(const RequestHandlerBase &other) = default;
    RequestHandlerBase &operator=(RequestHandlerBase &&other) = default;

    bool match(const std::string &_method, const std::string &_path) override;
    bool handle(const HttpRequest &, HttpResponse &) override { return false; };

    const std::string &path() const { return m_path; }
    const std::string &method() const { return m_method; }

  protected:
    regRetType regMatch(const std::regex &rgx, const std::string &s);
    bool hasMethod(const std::string &m);

  private:
    static int _pos(const char *s, size_t s_len, const char p);

    std::string m_method;
    std::string m_path;
};

} /* namespace http */

#endif /* COMPONENTS_HTTP_REQUESTHANDLERBASE_H_ */
