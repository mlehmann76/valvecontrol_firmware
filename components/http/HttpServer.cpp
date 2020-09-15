/*
 * HttpServer.cpp
 *
 *  Created on: 04.08.2020
 *      Author: marco
 */


#include "config_user.h"
#include <thread>
#include <esp_log.h>
#include <esp_pthread.h>
#include <freertos/task.h>
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "HttpServer.h"
#include "DefaultHandler.h"

#define TAG "HTTPSERVER"

namespace http {

HttpServer::HttpServer(int _port) :
		m_port(_port), m_socket(), m_sem("http server sem"), m_obs(
				new HttpServerConnectionObserver(this)) {
	//always have the defaultHandler in line
	m_pathhandler.push_back(std::make_shared<DefaultHandler>());
}

HttpServer::~HttpServer() {
	delete m_obs;
}

void HttpServer::task() {
	Socket &_s = socket();
	while (!_s.bind(port())) {
		//if bind failed, get a new socket
		_s.create();
		vTaskDelay(10);
	}
	if (_s.listen(0)) {
		_s.setNonBlocking(true);
	}

	while (1) {
		if (Socket::errorState == socket().pollConnectionState(std::chrono::microseconds(1))) {
			sem().give();
			return;
		}
		if (socket().hasNewConnection(std::chrono::microseconds(1))) {
			Socket *_con = socket().accept(std::chrono::microseconds(1));
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
						for (auto _p : m_pathhandler) {
							if (_p->match(req.method(), req.path())) {
								exit = !(_p->handle(req, resp));
								break;
							}
						}
						break;
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(20));
				} //while
				removeSocket(&_con);
			} //if
		} //if
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}


void HttpServer::start() {
	if (!m_sem.take(10 / portTICK_PERIOD_MS)) {
		ESP_LOGD(TAG, "server already running on start");
		return;
	}
	auto cfg = esp_pthread_get_default_config();
	cfg.thread_name = "http task";
	cfg.prio = 6;
	esp_pthread_set_cfg(&cfg);
	m_thread = std::thread([this]{task();});

}

void HttpServer::stop() {
	//close the main socket
	m_socket.close();
	//wait for task to stop
	m_sem.take(-1);
	m_sem.give();

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


} /* namespace http */
