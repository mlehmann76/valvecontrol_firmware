/*
 * HttpServer.cpp
 *
 *  Created on: 04.08.2020
 *      Author: marco
 */

#include "HttpServer.h"
#include "DefaultHandler.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include <esp_log.h>
#include <future>
#include <mutex>
#include <thread>

#define TAG "HTTPSERVER"

namespace http {

HttpServer::HttpServer(int _port, size_t maxCons, std::chrono::seconds _timeout)
    : m_port(_port), m_socket(), m_sem(),
      m_obs(new HttpServerConnectionObserver(this)), m_cons(),
      m_maxCons(maxCons), m_timeout(_timeout) {
    // always have the defaultHandler in line
    m_pathhandler.push_back(std::make_shared<DefaultHandler>());
    m_cons.reserve(maxCons);
}

HttpServer::~HttpServer() {
    stop();
    delete m_obs;
}

void HttpServer::setSocketTimeouts(const std::unique_ptr<Socket>& s,
		const std::chrono::seconds& sec) {
    struct timeval tv;
    tv.tv_sec = sec.count();
    tv.tv_usec = 0;
    s->setSocketOption(SO_RCVTIMEO, &tv, sizeof(tv));
    s->setSocketOption(SO_SNDTIMEO, &tv, sizeof(tv));
}

void HttpServer::task() {
    while (1) {
        if (Socket::errorState ==
            socket().pollConnectionState(std::chrono::milliseconds(10))) {
            sem().unlock();
            return;
        }
        if (m_maxCons > 1) {
            if (m_cons.size() < m_maxCons &&
                socket().hasNewConnection(std::chrono::milliseconds(10))) {
                std::unique_ptr<Socket> s(
                    socket().accept(std::chrono::milliseconds(10)));
                if (s.get() != nullptr) {
                    setSocketTimeouts(s, std::chrono::seconds(1));
                    m_cons.emplace_back(std::async(
                        &HttpServer::handleConnection, this, std::move(s)));
                }
            } // if
            // check if thread is done, remove thread
            for (auto i = 0; i < m_cons.size(); ++i) {
                if (is_ready(m_cons[i])) {
                    std::swap(m_cons[i], m_cons.back());
                    m_cons.pop_back();
                    break;
                } // if
            }     // for
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } else {
            if (socket().hasNewConnection(std::chrono::milliseconds(100))) {
                std::unique_ptr<Socket> s(
                    socket().accept(std::chrono::milliseconds(100)));
                setSocketTimeouts(s, std::chrono::seconds(1));
                handleConnection(std::move(s));
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        }
    }
}

void HttpServer::start() {
    if (!m_sem.try_lock()) {
        ESP_LOGD(TAG, "server already running on start");
        return;
    }
    Socket &_s = socket();
    while (!_s.bind(port())) {
        // if bind failed, get a new socket
        _s.create();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (_s.listen(m_maxCons > 0 ? m_maxCons - 1 : 0)) {
        _s.setNonBlocking(true);
    }

    m_thread = std::thread([this] { task(); });
}

void HttpServer::stop() {
    // close the main socket
    m_socket.close();
    // wait for task to stop
    m_sem.lock();
    m_sem.unlock();

    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void HttpServer::addPathHandler(PathHandlerType _p) {
    m_pathhandler.push_front(_p);
}

void HttpServer::remPathHandler(PathHandlerType _p) {
    for (auto it = m_pathhandler.begin(); it != m_pathhandler.end(); ++it) {
        if (*it == _p) {
            m_pathhandler.erase(it);
            break;
        }
    }
}

void HttpServer::handleConnection(std::unique_ptr<Socket> _con) {
    using namespace std::chrono;
    std::unique_ptr<HttpRequest> req = std::make_unique<HttpRequest>(&*_con);
    std::unique_ptr<HttpResponse> resp = std::make_unique<HttpResponse>(*req);
    auto _end = steady_clock::now() + m_timeout;
    bool exit = false;

    while (!exit && steady_clock::now() < _end) {
        switch (req->parse()) {
        case HttpRequest::PARSE_ERROR:
            exit = true;
            break;
        case HttpRequest::PARSE_NODATA:
            break;
        case HttpRequest::PARSE_OK:
            for (auto _p : m_pathhandler) {
                if (_p->match(req->method(), req->path())) {
                    (_p->handle(*req, *resp));
                    _end = steady_clock::now() + m_timeout;
                    break;
                }
            }
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    } // while
    _con->close();
}

} /* namespace http */
