/*
 * HttpServer.h
 *
 *  Created on: 04.08.2020
 *      Author: marco
 */

#ifndef MAIN_HTTPSERVER_H_
#define MAIN_HTTPSERVER_H_

#include "TaskCPP.h"
#include "SemaphoreCPP.h"
#include "socket.h"
#include "ConnectionObserver.h"

class HttpServerConnectionObserver;
class HttpServerTask;

class HttpServer {
	friend HttpServerTask;
public:
	HttpServer(int _port);
	virtual ~HttpServer();
	HttpServer(const HttpServer &other) = delete;
	HttpServer(HttpServer &&other) = delete;
	HttpServer& operator=(const HttpServer &other) = delete;
	HttpServer& operator=(HttpServer &&other) = delete;

	ConnectionObserver& obs();

	int port() {
		return m_port;
	}

	Socket& socket() {
		return m_socket;
	}

	Semaphore &sem() { return m_sem;}

	void start();
	void stop();

private:
	HttpServerTask *m_task;
	int m_port;
	Socket m_socket;
	Semaphore m_sem;
	HttpServerConnectionObserver *m_obs;
};

class HttpServerConnectionObserver: public ConnectionObserver {
public:
	HttpServerConnectionObserver(HttpServer *_m) :
			m_obs(_m) {
	}
	virtual void onConnect() {
		m_obs->start();
	}
	virtual void onDisconnect() {
		m_obs->stop();
	}
private:
	HttpServer *m_obs;
};

inline ConnectionObserver& HttpServer::obs() {
	return *m_obs;
}

#endif /* MAIN_HTTPSERVER_H_ */
