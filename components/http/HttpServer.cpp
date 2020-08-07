/*
 * HttpServer.cpp
 *
 *  Created on: 04.08.2020
 *      Author: marco
 */


#include "config_user.h"
#include <esp_log.h>
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "HttpServer.h"
#include "DefaultHandler.h"

#define TAG "HTTPSERVER"

namespace http {

HttpServer::HttpServer(int _port) :
		m_task(nullptr), m_port(_port), m_socket(), m_sem("http server sem"), m_obs(
				new HttpServerConnectionObserver(this)), m_defaultHandler(new DefaultHandler()) {
}

HttpServer::~HttpServer() {
	delete m_obs;
}

class HttpServerTask: public TaskClass {
public:
	HttpServerTask(HttpServer *_s) :
			TaskClass("http task", TaskPrio_HMI, 3072), m_httpServer(_s) {
	}
	~HttpServerTask() {
	}
	virtual void task();
private:
	HttpServer *m_httpServer;
};

void HttpServerTask::task() {
	Socket &_s = m_httpServer->socket();
	if (_s.bind(m_httpServer->port())) {
		if (_s.listen(2)) {
			_s.setNonBlocking(true);
			int value = 1;
			_s.setSocketOption(SO_REUSEADDR, &value, sizeof(value));

		}
	}
	while (1) {
		if (m_httpServer->socket().hasNewConnection(std::chrono::microseconds(1))) {
			Socket *_con = m_httpServer->socket().accept(std::chrono::microseconds(1));
			if (_con != nullptr) {
				ESP_LOGD(TAG, "socket(%d) accepted", _con->get());

				HttpRequest req;
				HttpResponse resp(*_con);

				while (_con != nullptr) {
					std::string _buf;
					switch (_con->pollConnectionState(std::chrono::microseconds(1))) {
					case Socket::noData:
						//ESP_LOGV(TAG, "Socket(%d)::noData", _con->get());
						break;
					case Socket::errorState:
						//ESP_LOGD(TAG, "Socket(%d)::errorState", _con->get());
						m_httpServer->removeSocket(&_con);
						break;
					case Socket::newData:
						int readSize = _con->read(_buf, 1024);
						ESP_LOGV(TAG, "Socket(%d)::newData (size %d) -> %s", _con->get(), readSize, _buf.c_str());
						if (readSize > 0) {
							bool handleRet = false;
							req.analyze(_buf);
							ESP_LOGD(TAG,"Req:%s uri:%s vers:%s", req.method().c_str(), req.path().c_str(), req.version().c_str());

							for (auto _p : m_httpServer->m_pathhandler) {
								if (_p->match(req.method(), req.path())) {
									handleRet = _p->handle(req, resp);
								}
							}
							if (false == handleRet) {
								m_httpServer->defaultHandler()->handle(req, resp);
								_con->write(resp.get(), resp.get().length());
								m_httpServer->removeSocket(&_con);
							}

						} else {
							m_httpServer->removeSocket(&_con);
						}
						break;
					}
					vTaskDelay(1);
				}
			}
			if (Socket::errorState == m_httpServer->socket().pollConnectionState(std::chrono::microseconds(1))) {
				m_httpServer->sem().give();
				return;
			}
		}
		vTaskDelay(1);
	}
}

void HttpServer::start() {
	if (!m_sem.take(10 / portTICK_PERIOD_MS)) {
		ESP_LOGD(TAG, "server already running on start");
		return;
	}
	if (m_task == nullptr) {
		m_task = new HttpServerTask(this);
	}
}

void HttpServer::stop() {
	//close the main socket
	m_socket.close();
	//wait for task to stop
	m_sem.take(-1);
	m_sem.give();

	if (m_task != nullptr) {
		delete m_task;
		m_task = nullptr;
	}
}

void HttpServer::addPathHandler(RequestHandlerBase *_p) {
	m_pathhandler.push_back(_p);
}

void HttpServer::remPathHandler(RequestHandlerBase *_p) {
	for (std::vector<RequestHandlerBase*>::iterator it = m_pathhandler.begin(); it != m_pathhandler.end(); ++it) {
		if (*it == _p) {
			m_pathhandler.erase(it);
			break;
		}
	}
}

void HttpServer::removeSocket(Socket **_s) {
	(*_s)->close();
	delete (*_s);
	*_s = nullptr;
}

} /* namespace http */
