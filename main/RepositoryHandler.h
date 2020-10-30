/*
 * RepositoryHandler.h
 *
 *  Created on: 13.09.2020
 *      Author: marco
 */

#ifndef MAIN_REPOSITORYHANDLER_H_
#define MAIN_REPOSITORYHANDLER_H_

#include "RequestHandlerBase.h"
#include <map>
#include <string>

class repository;

namespace http {

class RepositoryHandler : public RequestHandlerBase {
  public:
    RepositoryHandler(const std::string &_method, const std::string &_path);
    virtual ~RepositoryHandler() {
        // TODO Auto-generated destructor stub
    }
    RepositoryHandler(const RepositoryHandler &other) = delete;
    RepositoryHandler(RepositoryHandler &&other) = default;
    RepositoryHandler &operator=(const RepositoryHandler &other) = default;
    RepositoryHandler &operator=(RepositoryHandler &&other) = default;
    // from RequestHandlerBase
    virtual bool handle(const HttpRequest &, HttpResponse &);
    /*
     *
     */
    void add(const std::string &name, const repository &rep) {
        m_repositories[name] = &rep;
    }

  private:
    std::map<std::string, const repository *> m_repositories;
};

} /* namespace http */

#endif /* MAIN_REPOSITORYHANDLER_H_ */
