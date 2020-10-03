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

#include "../../main/iConnectionObserver.h"
#include "socket.h"
#include "SemaphoreCPP.h"

namespace http {

class HttpServerConnectionObserver;
class iHandler;
class RequestHandlerBase;
class DefaultHandler;

class HttpServer {

	using PathHandlerType = std::shared_ptr<iHandler>;
	using PathHandlerSetType = std::list<PathHandlerType>;

public:
	HttpServer(int _port);
	virtual ~HttpServer();
	HttpServer(const HttpServer &other) = delete;
	HttpServer(HttpServer &&other) = delete;
	HttpServer& operator=(const HttpServer &other) = delete;
	HttpServer& operator=(HttpServer &&other) = delete;

	iConnectionObserver& obs();

	int port() {
		return m_port;
	}

	Socket& socket() {
		return m_socket;
	}

	Semaphore &sem() { return m_sem;}

	void start();
	void stop();

	void addPathHandler(PathHandlerType );
	void remPathHandler(PathHandlerType );

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
	PathHandlerSetType m_pathhandler;
};

class HttpServerConnectionObserver: public iConnectionObserver {
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

inline iConnectionObserver& HttpServer::obs() {
	return *m_obs;
}

} /* namespace http */

#endif /* COMPONENTS_HTTP_HTTPSERVER_H_ */
