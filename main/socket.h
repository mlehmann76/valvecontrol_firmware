/*
 * socket.h
 *
 *  Created on: 31.07.2020
 *      Author: marco
 */

#ifndef MAIN_SOCKET_H_
#define MAIN_SOCKET_H_

#include <string>
#include <chrono>
#include <sys/socket.h>

#define SocketSetSize 8

class Socket {
public:
	typedef std::chrono::microseconds TimeoutValue;
	enum PollType { newData, errorState, noData };

	Socket(int = -1, bool _tcp = true);
	Socket(const Socket&) = delete;
	Socket(Socket&&) = delete;
	~Socket() = default;
	Socket& operator =(const Socket&) = delete;
	Socket& operator =(Socket&&) = delete;

	Socket* accept(const TimeoutValue &timeout);

	bool bind(int port, struct in_addr adr = {IPADDR_ANY});

	void close();

	bool connect(std::string hostname, unsigned short remotePort, const TimeoutValue &timeout);

	bool listen(unsigned short backlog);

	int read(char *buffer, int size);
	int read(std::string &buffer, int size);

	int write(const std::string &buffer, int size);
	int write(const char *buffer, int size);

	PollType pollConnectionState(const TimeoutValue &timeout);

	bool setSocketOption(int option, void* value, size_t len);

	bool setNonBlocking(bool state);

	bool hasNewConnection(const TimeoutValue &timeout);

	bool IsConnected();

private:
	struct timeval toTimeVal(const TimeoutValue &timeout) {
		struct timeval _timeout;
		_timeout.tv_usec = timeout.count();
		return _timeout;
	}

	int m_socket;
	bool m_isTCP;
	bool m_isConnected;
};


#endif /* MAIN_SOCKET_H_ */
