/*
 * HttpServer.h
 *
 *  Created on: 04.08.2020
 *      Author: marco
 */

#ifndef COMPONENTS_HTTP_HTTPSERVER_H_
#define COMPONENTS_HTTP_HTTPSERVER_H_

#include <list>
#include <thread>
#include "SemaphoreCPP.h"
#include "socket.h"
#include "ConnectionObserver.h"

namespace http {

class HttpServerConnectionObserver;
class RequestHandlerBase;
class DefaultHandler;

class HttpServer {
	using PathHandlerType = std::list<RequestHandlerBase*>;

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

	void addPathHandler(RequestHandlerBase *);
	void remPathHandler(RequestHandlerBase *);

private:

	virtual void task();

	void removeSocket(Socket **_s) {
		(*_s)->close();
		delete (*_s);
		*_s = nullptr;
	}

	std::thread m_thread;
	int m_port;
	Socket m_socket;
	Semaphore m_sem;
	HttpServerConnectionObserver *m_obs;
	PathHandlerType m_pathhandler;
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
