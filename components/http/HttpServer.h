/*
 * HttpServer.h
 *
 *  Created on: 04.08.2020
 *      Author: marco
 */

#ifndef COMPONENTS_HTTP_HTTPSERVER_H_
#define COMPONENTS_HTTP_HTTPSERVER_H_

#include <vector>
#include "TaskCPP.h"
#include "SemaphoreCPP.h"
#include "socket.h"
#include "ConnectionObserver.h"

namespace http {

class HttpServerConnectionObserver;
class HttpServerTask;
class RequestHandlerBase;
class DefaultHandler;

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
	DefaultHandler *defaultHandler() {return m_defaultHandler; }

	void start();
	void stop();

	void addPathHandler(RequestHandlerBase *);
	void remPathHandler(RequestHandlerBase *);

private:
	void removeSocket(Socket **_s);

	HttpServerTask *m_task;
	int m_port;
	Socket m_socket;
	Semaphore m_sem;
	HttpServerConnectionObserver *m_obs;
	std::vector<RequestHandlerBase*> m_pathhandler;
	DefaultHandler *m_defaultHandler;
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

} /* namespace http */

#endif /* COMPONENTS_HTTP_HTTPSERVER_H_ */
