/*
 * HttpServer.cpp
 *
 *  Created on: 04.08.2020
 *      Author: marco
 */

#include <esp_log.h>
#include "config_user.h"
#include "HttpServer.h"

#define TAG "HttpServer"

HttpServer::HttpServer(int _port) :
		m_task(nullptr), m_port(_port), m_socket(), m_sem("http server sem"), m_obs(
				new HttpServerConnectionObserver(this)) {
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

			}
			/**
			 *
			 */
			if (Socket::errorState == m_httpServer->socket().pollConnectionState(std::chrono::microseconds(1))) {
				m_httpServer->sem().give();
				return;
			}
			_con->close();
			delete _con;
		}
		taskYIELD();
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
