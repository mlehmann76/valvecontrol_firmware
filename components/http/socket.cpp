/*
 * socket.cpp
 *
 *  Created on: 31.07.2020
 *      Author: marco
 */

#include "socket.h"
#include <fcntl.h>
#include <memory>
#include <netdb.h>
#include <strings.h>
#include <unistd.h>

#define TAG "SOCKET"

Socket::Socket(int _s, SocketType _type)
    : m_socket(_s), m_socketType(_type), m_isConnected(false) {
    if (m_socket == -1) {
        create();
    }
}

void Socket::create() {
    m_socket = ::socket(AF_INET, m_socketType, 0);
    if (m_socket == -1) {
        // ESP_LOGE(TAG, "error creating Socket");
    }
}

Socket *Socket::accept(const TimeoutValue &timeout) {
    fd_set set;
    int newSocketHandle = 0;
    struct sockaddr_in client;
    unsigned int len = sizeof(client);

    //  Clear master sockets set
    FD_ZERO(&set);

    //  Add listening socket to the master set
    FD_SET(m_socket, &set);
    struct timeval _timeout = toTimeVal(timeout);

    if (::select(SocketSetSize, &set, 0, &set, &_timeout) > 0) {
        newSocketHandle = ::accept(m_socket, (struct sockaddr *)&client, &len);

        if (newSocketHandle == -1) {
            // ESP_LOGD(TAG, "error on accept(%d) -> %s", m_socket,
            // strerror(errno));
            return nullptr;
        }

        return new Socket(newSocketHandle, m_socketType);
    } else {
        // ESP_LOGE(TAG, "error on select %s", strerror(errno));
    }

    return nullptr;
}

bool Socket::bind(int port, struct in_addr adr) {
    // ESP_LOGD(TAG, "bind: port=%d", port);

    if (m_socket == -1) {
        // ESP_LOGE(TAG, "bind: Socket is not initialized.");
    }

    struct sockaddr_in _serv_addr;
    bzero((void *)&_serv_addr, sizeof(_serv_addr));

    _serv_addr.sin_family = AF_INET;
    _serv_addr.sin_addr.s_addr = adr.s_addr;
    _serv_addr.sin_port = htons(port);
    int _b =
        ::bind(m_socket, (struct sockaddr *)&_serv_addr, sizeof(_serv_addr));
    if (_b < 0) {
        // ESP_LOGE(TAG, "ERROR on binding %s", strerror(errno));
        return false;
    } else {
        return true;
    }
}

void Socket::close() {
    // ESP_LOGD(TAG, "socket %d close", m_socket);
    if (m_socket != -1) {
        ::close(m_socket);
        m_isConnected = false;
    }
}

bool Socket::connect(std::string hostname, unsigned short remotePort,
                     const TimeoutValue &timeout) {
    bool connectSuccess = false;
    // ESP_LOGD(TAG, "connect %s, port %d", hostname.c_str(), remotePort);
    if (m_socket == -1) {
        // ESP_LOGE(TAG, "connect: Socket is not initialized.");
    }
    std::string service = std::to_string(remotePort);
    struct addrinfo getaddrinfoHints;
    struct addrinfo *getResults;
    getaddrinfoHints.ai_family = AF_INET;
    getaddrinfoHints.ai_socktype = SOCK_STREAM;

    if (::getaddrinfo(hostname.c_str(), service.c_str(), &getaddrinfoHints,
                      &getResults) != 0) {
        // ESP_LOGE(TAG, "error on getaddrinfo %s,%s", hostname.c_str(),
        // service.c_str());
        freeaddrinfo(getResults);
        return false;
    }
    struct sockaddr_in *addrData;
    addrData = (struct sockaddr_in *)&(*getResults->ai_addr->sa_data);

    int connReturn = ::connect(m_socket, (const struct sockaddr *)(addrData),
                               (int)getResults->ai_addrlen);

    if (connReturn == -1) {
        // ESP_LOGE(TAG, "error on connect %d", connReturn);
        int m_lastErrorCode = errno;

        //  Connection error : FATAL
        if ((m_lastErrorCode != EWOULDBLOCK) && (m_lastErrorCode != EALREADY)) {
            connectSuccess = false;
        } else {
            fd_set writeFDS;
            fd_set exceptFDS;
            int selectReturn = 0;

            //  Clear all the socket FDS structures
            FD_CLR(m_socket, &writeFDS);
            FD_CLR(m_socket, &exceptFDS);

            //  Put the socket into the FDS structures
            FD_SET(m_socket, &writeFDS);
            FD_SET(m_socket, &exceptFDS);

            struct timeval _timeout = toTimeVal(timeout);
            selectReturn = ::select(-1, NULL, &writeFDS, &exceptFDS, &_timeout);

            if (selectReturn == -1) {
                // ESP_LOGE(TAG, "error on select %s", strerror(errno));
                connectSuccess = false;
            } else if (selectReturn > 0) {
                if (FD_ISSET(m_socket, &exceptFDS)) {
                    connectSuccess = false;
                } else if (FD_ISSET(m_socket, &writeFDS)) {
                    connectSuccess = true;
                    m_isConnected = true;
                }
            }
        }
    } else {
        connectSuccess = true;
        m_isConnected = true;
    }
    freeaddrinfo(getResults);
    return connectSuccess;
}

bool Socket::listen(unsigned short backlog) {
    // ESP_LOGD(TAG, "socket %d listen, backlog %d", m_socket, backlog);
    if (m_socket == -1) {
        // ESP_LOGE(TAG, "listen: Socket is not initialized.");
    }
    int lret = ::listen(m_socket, backlog);
    if (lret < 0) {
        // ESP_LOGE(TAG, "ERROR on listen %d", lret);
        return false;
    } else {
        return true;
    }
}

int Socket::read(char *buffer, size_t size) {
    if (m_socket == -1) {
        // ESP_LOGE(TAG, "read: Socket is not initialized.");
    }
    int recReturn = ::recv(m_socket, buffer, size, 0);
    if (recReturn == -1) {
        if (errno == EWOULDBLOCK) {
            recReturn = 0;
        } else {
            // ESP_LOGE(TAG, "receive: %s", strerror(errno));
        }
    }
    return recReturn;
}

int Socket::read(std::string &buffer, size_t size) {
    std::unique_ptr<char[]> _buf(new char[size]);
    int ret = read(_buf.get(), size);
    buffer = std::string(ret > 0 ? _buf.get() : "", ret > 0 ? ret : 0);
    return ret;
}

int Socket::write(const std::string &buffer, size_t size) {
    return write(buffer.c_str(), size);
}

int Socket::write(const char *buffer, size_t size) {
    if (m_socket == -1) {
        // ESP_LOGE(TAG, "write: Socket is not initialized.");
    }
    int writeReturn = ::send(m_socket, buffer, size, 0);
    if (writeReturn == -1) {
        if (errno == EWOULDBLOCK) {
            writeReturn = 0;
        } else {
            // ESP_LOGE(TAG, "write: %s", strerror(errno));
        }
    } else {
        // ESP_LOGD(TAG, "written: %s", buffer);
    }
    return writeReturn;
}

Socket::PollType Socket::pollConnectionState(const TimeoutValue &timeout) {
    PollType returnValue = newData;
    fd_set socketSet;
    int selectState = 0;

    FD_ZERO(&socketSet);
    FD_SET(m_socket, &socketSet);

    struct timeval _timeout = toTimeVal(timeout);
    selectState = ::select(m_socket + 1, &socketSet, NULL, NULL, &_timeout);

    if (selectState == -1) {
        //  Error state : 'SOCKET_ERROR'
        returnValue = errorState;
    } else if (selectState == 0) {
        //  Poll result of '0' means timeout or no data.
        returnValue = noData;
    } else {
        //  Otherwise new data.
        returnValue = newData;
    }

    return returnValue;
}

bool Socket::setSocketOption(int option, void *value, size_t len) {
    int res = ::setsockopt(m_socket, SOL_SOCKET, option, value, len);
    if (res < 0) {
        // ESP_LOGE(TAG, "setSocketOption -> %X : %d", option, errno);
    }
    return res < 0 ? false : true;
}

bool Socket::hasNewConnection(const TimeoutValue &timeout) {
    fd_set read_fd_set;

    FD_ZERO(&read_fd_set);
    FD_SET(m_socket, &read_fd_set);

    struct timeval _timeout = toTimeVal(timeout);
    if (::select(SocketSetSize, &read_fd_set, NULL, NULL, &_timeout) < 0) {
        // ESP_LOGE(TAG, "::select() failed, unable to continue.");
    }

    if (FD_ISSET(m_socket, &read_fd_set)) {
        return true;
    }

    return false;
}

bool Socket::setNonBlocking(bool state) {
    int ret = -1;
    // https://stackoverflow.com/questions/1150635/unix-nonblocking-i-o-o-nonblock-vs-fionbio
    int flags = ::fcntl(m_socket, F_GETFL, 0);

    if (state) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    ret = ::fcntl(m_socket, F_SETFL, flags);
    if (ret == -1) {
        // ESP_LOGE(TAG, "::fcntl() failed");
        return false;
    }
    return true;
}

bool Socket::IsConnected() { return m_isConnected; }

Socket::~Socket() {
    // ESP_LOGD(TAG, "connect %s, port %d", hostname.c_str(), remotePort);
}
