/*
 * echoServer.h
 *
 *  Created on: 01.08.2020
 *      Author: marco
 */

#ifndef MAIN_ECHOSERVER_H_
#define MAIN_ECHOSERVER_H_

#include "TaskCPP.h"
#include "socket.h"
#include <vector>

class EchoServer : public TaskClass{
public:
	EchoServer();
	virtual ~EchoServer();
	EchoServer(EchoServer &&other) = delete;
	EchoServer(const EchoServer &other) = delete;
	EchoServer& operator=(const EchoServer &other) = delete;
	EchoServer& operator=(EchoServer &&other) = delete;
	virtual void task();
	void start();
private:
	std::vector<Socket*> m_sockets;

	void removeSocket(Socket *_s);
};

#endif /* MAIN_ECHOSERVER_H_ */
