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
				new HttpServerConnectionObserver(this)) {
	//always have the defaultHandler in line
	m_pathhandler.push_back(new DefaultHandler);
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

	void removeSocket(Socket **_s) {
		(*_s)->close();
		delete (*_s);
		*_s = nullptr;
	}

private:
	HttpServer *m_httpServer;
};

void HttpServerTask::task() {
	Socket &_s = m_httpServer->socket();
	while (!_s.bind(m_httpServer->port())) {
		//if bind failed, get a new socket
		_s.create();
		vTaskDelay(10);
	}
	if (_s.listen(0)) {
		_s.setNonBlocking(true);
	}

	while (1) {
		if (Socket::errorState == m_httpServer->socket().pollConnectionState(std::chrono::microseconds(1))) {
			m_httpServer->sem().give();
			return;
		}
		if (m_httpServer->socket().hasNewConnection(std::chrono::microseconds(1))) {
			Socket *_con = m_httpServer->socket().accept(std::chrono::microseconds(1));
			if (_con != nullptr) {
				ESP_LOGD(TAG, "socket(%d) accepted", _con->get());

				HttpRequest req(_con);
				HttpResponse resp(req);

				bool exit = false;
				while (!exit) {
					switch (req.parse()) {
					case HttpRequest::PARSE_ERROR:
						exit = true;
						break;
					case HttpRequest::PARSE_NODATA:
						break;
					case HttpRequest::PARSE_OK:
						for (auto _p : m_httpServer->m_pathhandler) {
							if (_p->match(req.method(), req.path())) {
								exit = !(_p->handle(req, resp));
								break;
							}
						}
						break;
					}
					vTaskDelay(1);
				} //while
				removeSocket(&_con);
			} //if
		} //if
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
	m_pathhandler.push_front(_p);
}

void HttpServer::remPathHandler(RequestHandlerBase *_p) {
	for (PathHandlerType::iterator it = m_pathhandler.begin(); it != m_pathhandler.end(); ++it) {
		if (*it == _p) {
			m_pathhandler.erase(it);
			break;
		}
	}
}


} /* namespace http */
