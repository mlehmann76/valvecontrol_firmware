/*
 * iHandler.h
 *
 *  Created on: 16.09.2020
 *      Author: marco
 */

#ifndef COMPONENTS_HTTP_IHANDLER_H_
#define COMPONENTS_HTTP_IHANDLER_H_

#include <string>

namespace http {
class HttpRequest;
class HttpResponse;

class iHandler {
public:
	virtual ~iHandler() = default;
	virtual bool match(const std::string &_method, const std::string &_path) = 0;
	virtual bool handle(const HttpRequest &, HttpResponse &) = 0;
};

} /* namespace http */

#endif /* COMPONENTS_HTTP_IHANDLER_H_ */
