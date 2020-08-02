/*
 * echoServer.cpp
 *
 *  Created on: 01.08.2020
 *      Author: marco
http://openbook.rheinwerk-verlag.de/linux_unix_programmierung/Kap11-013.htm#RxxKap11013040003921F03B100
 */


#include "config_user.h"
#include <strings.h>
#include <unistd.h>
#include <memory>
#include <netdb.h>
#include <esp_log.h>
#include "echoServer.h"

#define TAG "ECHO"

EchoServer::EchoServer() : TaskClass("status", TaskPrio_HMI, 2048), m_sockets() {
}

EchoServer::~EchoServer() {
}

void EchoServer::removeSocket(Socket *_s) {
	for (std::vector<Socket*>::iterator it = m_sockets.begin(); it != m_sockets.end(); ++it) {
		if ((*it)->get() == _s->get()) {
			(*it)->close();
			delete (*it);
			m_sockets.erase(it);
			break;
		}
	}
}

void EchoServer::task() {
	while (1) {
		if (m_sockets.size() > 0 && m_sockets.back() != nullptr) {
			if (m_sockets.front()->hasNewConnection(std::chrono::milliseconds(100))) {
				Socket *_con = m_sockets.front()->accept(std::chrono::milliseconds(100));
				if (_con != nullptr) {
					ESP_LOGD(TAG, "socket accepted");
					m_sockets.push_back(_con);
				}
			}
			for (size_t i = 1; i < m_sockets.size(); i++) {
				Socket *_s = m_sockets.at(i);
				if ((_s != nullptr)) {
					char buf[128];
					switch (_s->pollConnectionState(std::chrono::microseconds(1))) {
					case Socket::noData:
						ESP_LOGD(TAG, "Socket(%d)::noData",_s->get());
						break;
					case Socket::errorState:
						ESP_LOGD(TAG, "Socket(%d)::errorState",_s->get());
						removeSocket(_s);
						break;
					case Socket::newData:
						ESP_LOGD(TAG, "Socket(%d)::newData",_s->get());
						bzero(buf, sizeof(buf));
						int readSize = _s->read(buf, sizeof(buf));
						if (readSize >= 0) {
							ESP_LOGD(TAG, "read: %d , %s", readSize, buf);
							_s->write(buf, readSize);
						} else {
							removeSocket(_s);
						}
						break;
					}
				}
			}
		}
		vTaskDelay(1);
	}
}

void EchoServer::start() {
	Socket *_s = new Socket();
	if (_s->bind(81)) {
		if (_s->listen(8)) {
			_s->setNonBlocking(true);
			int value = 1;
			_s->setSocketOption(SO_REUSEADDR, &value, sizeof(value));
			m_sockets.push_back(_s);
		}
	}
}
