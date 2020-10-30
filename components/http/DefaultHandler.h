/*
 * DefaultHandler.h
 *
 *  Created on: 07.08.2020
 *      Author: marco
 */

#ifndef COMPONENTS_HTTP_DEFAULTHANDLER_H_
#define COMPONENTS_HTTP_DEFAULTHANDLER_H_

#include "RequestHandlerBase.h"

namespace http {

class DefaultHandler : public RequestHandlerBase {
  public:
    DefaultHandler();
    virtual ~DefaultHandler();
    DefaultHandler(const DefaultHandler &other) = default;
    DefaultHandler(DefaultHandler &&other) = default;
    DefaultHandler &operator=(const DefaultHandler &other) = default;
    DefaultHandler &operator=(DefaultHandler &&other) = default;
    // from RequestHandlerBase
    virtual bool match(const std::string &, const std::string &);
    virtual bool handle(const HttpRequest &, HttpResponse &);
};

} /* namespace http */

#endif /* COMPONENTS_HTTP_DEFAULTHANDLER_H_ */
