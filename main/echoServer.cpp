/*
 * echoServer.cpp
 *
 *  Created on: 01.08.2020
 *      Author: marco
http://openbook.rheinwerk-verlag.de/linux_unix_programmierung/Kap11-013.htm#RxxKap11013040003921F03B100
 */


#include "config_user.h"
#include <unistd.h>
#include <memory>
#include <netdb.h>
#include <esp_log.h>
#include <esp_pthread.h>

#include "echoServer.h"

#define TAG "ECHO"

EchoServer::EchoServer() : m_sockets(), m_obs(new EchoConnectionObserver(this)) {
}

EchoServer::~EchoServer() {
	delete m_obs;
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
		if (m_sockets.size() > 0 && m_sockets.front() != nullptr) {
			if (m_sockets.front()->hasNewConnection(std::chrono::microseconds(1))) {
				Socket *_con = m_sockets.front()->accept(std::chrono::microseconds(1));
				if (_con != nullptr) {
					ESP_LOGD(TAG, "socket(%d) accepted", _con->get());
					m_sockets.push_back(_con);
				}
			}
			for (size_t i = 1; i < m_sockets.size(); i++) {
				Socket *_s = m_sockets.at(i);
				if ((_s != nullptr)) {
					char buf[1024];
					switch (_s->pollConnectionState(std::chrono::microseconds(1))) {
					case Socket::noData:
						ESP_LOGV(TAG, "Socket(%d)::noData",_s->get());
						break;
					case Socket::errorState:
						ESP_LOGV(TAG, "Socket(%d)::errorState",_s->get());
						removeSocket(_s);
						break;
					case Socket::newData:
						ESP_LOGV(TAG, "Socket(%d)::newData",_s->get());
						bzero(buf, sizeof(buf));
						int readSize = _s->read(buf, sizeof(buf));
						if (readSize >= 0) {
							ESP_LOGD(TAG, "socket(%d) read: %d , %s",_s->get(), readSize, buf);
							_s->write(buf, readSize);
						} else {
							removeSocket(_s);
						}
						break;
					}
				}
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(15));
	}
}

void EchoServer::start() {
	Socket *_s = new Socket();
	if (_s->bind(81)) {
		if (_s->listen(2)) {
			_s->setNonBlocking(true);
			int value = 1;
			_s->setSocketOption(SO_REUSEADDR, &value, sizeof(value));
			m_sockets.push_back(_s);
			auto cfg = esp_pthread_get_default_config();
				cfg.thread_name = "echo task";
				esp_pthread_set_cfg(&cfg);
				m_thread = std::thread([this](){this->task();});
		}
	}
}
