/*
 * echoServer.h
 *
 *  Created on: 01.08.2020
 *      Author: marco
 */

#ifndef MAIN_ECHOSERVER_H_
#define MAIN_ECHOSERVER_H_

#include <thread>
#include "MutexCPP.h"
#include "socket.h"
#include <vector>
#include "iConnectionObserver.h"

class EchoServer;
class EchoConnectionObserver : public iConnectionObserver {
public:
	EchoConnectionObserver(EchoServer *_m) : m_obs(_m) {}
	virtual void onConnect();
	virtual void onDisconnect() {}
private:
	EchoServer *m_obs;
};
class EchoServer {
public:
	EchoServer();
	virtual ~EchoServer();
	EchoServer(EchoServer &&other) = delete;
	EchoServer(const EchoServer &other) = delete;
	EchoServer& operator=(const EchoServer &other) = delete;
	EchoServer& operator=(EchoServer &&other) = delete;
	virtual void task();
	void start();
	iConnectionObserver &obs() {return *m_obs;}
private:
	void removeSocket(Socket *_s);

	std::thread m_thread;
	std::vector<Socket*> m_sockets;
	EchoConnectionObserver *m_obs;
};

inline void EchoConnectionObserver::onConnect() {
	m_obs->start();
}

#endif /* MAIN_ECHOSERVER_H_ */
