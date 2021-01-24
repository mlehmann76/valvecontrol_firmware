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
#include <future>
#include <mutex>
#include <thread>

#define TAG "HTTPSERVER"

namespace http {

HttpServer::HttpServer(int _port, size_t maxCons)
    : m_port(_port), m_socket(), m_sem(),
      m_obs(new HttpServerConnectionObserver(this)), m_cons(),
      m_maxCons(maxCons) {
    // always have the defaultHandler in line
    m_pathhandler.push_back(std::make_shared<DefaultHandler>());
    m_cons.reserve(maxCons);
}

HttpServer::~HttpServer() { delete m_obs; }

void HttpServer::task() {
    Socket &_s = socket();
    while (!_s.bind(port())) {
        // if bind failed, get a new socket
        _s.create();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (_s.listen(m_maxCons > 0 ? m_maxCons - 1 : 0)) {
        _s.setNonBlocking(true);
    }

    while (1) {
        if (Socket::errorState ==
            socket().pollConnectionState(std::chrono::milliseconds(10))) {
            sem().unlock();
            return;
        }
#if 0
        if (m_cons.size() < m_maxCons &&
            socket().hasNewConnection(std::chrono::milliseconds(10))) {
            std::unique_ptr<Socket> s(
                socket().accept(std::chrono::milliseconds(10)));
            m_cons.emplace_back(
                std::async(&HttpServer::handleConnection, this, std::move(s)));
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
#else
        if (socket().hasNewConnection(std::chrono::milliseconds(100))) {
        	std::unique_ptr<Socket> s(
        	                socket().accept(std::chrono::milliseconds(10)));
        	handleConnection(std::move(s));
        } else {
        	std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
#endif
    }
}

void HttpServer::start() {
    if (!m_sem.try_lock()) {
        // ESP_LOGD(TAG, "server already running on start");
        return;
    }
    //	auto cfg = esp_pthread_get_default_config();
    //	cfg.thread_name = "http task";
    //	cfg.prio = 6;
    //	esp_pthread_set_cfg(&cfg);
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
    std::unique_ptr<HttpRequest> req = std::make_unique<HttpRequest>(&*_con);
    std::unique_ptr<HttpResponse> resp = std::make_unique<HttpResponse>(*req);

    bool exit = false;
    while (!exit) {
        switch (req->parse()) {
        case HttpRequest::PARSE_ERROR:
            exit = true;
            break;
        case HttpRequest::PARSE_NODATA:
            break;
        case HttpRequest::PARSE_OK:
            for (auto _p : m_pathhandler) {
                if (_p->match(req->method(), req->path())) {
                    exit = !(_p->handle(*req, *resp));
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
