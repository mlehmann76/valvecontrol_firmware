/*
 * HttpServer.h
 *
 *  Created on: 04.08.2020
 *      Author: marco
 */

#ifndef COMPONENTS_HTTP_HTTPSERVER_H_
#define COMPONENTS_HTTP_HTTPSERVER_H_

#include <future>
#include <list>
#include <mutex>
#include <thread>
#include <vector>

#include "../../main/iConnectionObserver.h"
#include "socket.h"

namespace http {

class HttpServerConnectionObserver;
class iHandler;
class RequestHandlerBase;
class DefaultHandler;

class HttpServer {

    using PathHandlerType = std::shared_ptr<iHandler>;
    using PathHandlerSetType = std::list<PathHandlerType>;

  public:
    HttpServer(int _port, size_t maxCons = 8,
               std::chrono::seconds _timeout = std::chrono::seconds(5));
    virtual ~HttpServer();
    HttpServer(const HttpServer &other) = delete;
    HttpServer(HttpServer &&other) = delete;
    HttpServer &operator=(const HttpServer &other) = delete;
    HttpServer &operator=(HttpServer &&other) = delete;

    iConnectionObserver &obs();

    int port() { return m_port; }

    Socket &socket() { return m_socket; }

    std::mutex &sem() { return m_sem; }

    void start();
    void stop();

    void addPathHandler(PathHandlerType);
    void remPathHandler(PathHandlerType);

  private:
    virtual void task();
    void handleConnection(std::unique_ptr<Socket> _con);
    void setSocketTimeouts(const std::unique_ptr<Socket> &s,
                           const std::chrono::seconds &sec);

    template <typename R> bool is_ready(std::future<R> const &f) {
        return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }

    std::thread m_thread;
    int m_port;
    Socket m_socket;
    std::mutex m_sem;
    HttpServerConnectionObserver *m_obs;
    PathHandlerSetType m_pathhandler;
    std::vector<std::future<void>> m_cons;
    const size_t m_maxCons;
    const std::chrono::seconds m_timeout;
};

class HttpServerConnectionObserver : public iConnectionObserver {
  public:
    HttpServerConnectionObserver(HttpServer *_m) : m_obs(_m) {}
    virtual void onConnect() { m_obs->start(); }
    virtual void onDisconnect() { m_obs->stop(); }

  private:
    HttpServer *m_obs;
};

inline iConnectionObserver &HttpServer::obs() { return *m_obs; }

} /* namespace http */

#endif /* COMPONENTS_HTTP_HTTPSERVER_H_ */
