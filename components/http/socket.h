/*
 * socket.h
 *
 *  Created on: 31.07.2020
 *      Author: marco
 */

#ifndef COMPONENTS_HTTP_SOCKET_H_
#define COMPONENTS_HTTP_SOCKET_H_

#include <chrono>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>

#define SocketSetSize FD_SETSIZE
#ifndef IPADDR_ANY
#define IPADDR_ANY ((uint32_t)0x00000000UL)
#endif

class Socket {
  public:
    typedef std::chrono::microseconds TimeoutValue;
    enum PollType { newData, errorState, noData };
    enum SocketType { SOCKET_STREAM = 1, SOCKET_DGRAM = 2 };

    Socket(int = -1, SocketType = SOCKET_STREAM);
    Socket(const Socket &) = delete;
    Socket(Socket &&) = delete;
    ~Socket();
    Socket &operator=(const Socket &) = delete;
    Socket &operator=(Socket &&) = delete;

    Socket *accept(const TimeoutValue &timeout);

    bool bind(int port, struct in_addr adr = {IPADDR_ANY});

    void close();

    bool connect(std::string hostname, unsigned short remotePort,
                 const TimeoutValue &timeout);

    bool listen(unsigned short backlog);

    int read(char *buffer, size_t size);
    int read(std::string &buffer, size_t size);

    int write(const std::string &buffer, size_t size);
    int write(const char *buffer, size_t size);

    PollType pollConnectionState(const TimeoutValue &timeout);

    bool setSocketOption(int option, void *value, size_t len);

    bool setNonBlocking(bool state);

    bool hasNewConnection(const TimeoutValue &timeout);

    bool IsConnected();

    int get() { return m_socket; }

    void create();

  private:
    struct timeval toTimeVal(const TimeoutValue &timeout) {
        struct timeval _timeout;
        _timeout.tv_sec = 0;
        _timeout.tv_usec = timeout.count();
        return _timeout;
    }

    int m_socket;
    SocketType m_socketType;
    bool m_isConnected;
};

#endif /* COMPONENTS_HTTP_SOCKET_H_ */
