/*
 * FileHandler.h
 *
 *  Created on: 29.10.2020
 *      Author: marco
 */

#ifndef COMPONENTS_HTTP_FILEHANDLER_H_
#define COMPONENTS_HTTP_FILEHANDLER_H_

#include "RequestHandlerBase.h"

namespace http {

class FileHandler : public RequestHandlerBase {
    static const size_t s_maxBlockSize = 8192;

  public:
    FileHandler(const std::string &_method, const std::string &_path,
                const std::string &_base);
    virtual ~FileHandler();

    // bool match(const std::string &_method, const std::string &_path)
    // override;
    bool handle(const HttpRequest &, HttpResponse &) override;

  private:
    bool doGet(const HttpRequest &_req, HttpResponse &_res);

    const std::string m_base;
};

} /* namespace http */

#endif /* COMPONENTS_HTTP_FILEHANDLER_H_ */
