#ifndef TEST_PORT_HPP
#define TEST_PORT_HPP

/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <cstring>
#include <cerrno>

/* Some simple platform-secifics to acquire an unused socket */

#if defined(_WIN32)

extern "C" {
# include <winsock2.h>
# include <ws2tcpip.h>
}

typedef SOCKET sock_t;

void check_err(int ret, const char *what) {
  if (ret) {
    char buf[512];
    FormatMessage(
      FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL, WSAGetLastError(), NULL, buf, sizeof(buf), NULL);
    fprintf(stderr, "%s: %s\n", what, buf);
    throw std::runtime_error(buf);
  }
}

class test_socket {
  public:
    SOCKET sock_;
    test_socket() {
        WORD wsa_ver = MAKEWORD(2, 2);
        WSADATA unused;
        check_err(WSAStartup(wsa_ver, &unused), "WSAStartup");
        sock_ = socket(AF_INET, SOCK_STREAM, 0);
        check_err(sock_ < 0, "socket");
    }
    ~test_socket() { WSACleanup(); }
    void close_early()  { closesocket(sock_); } // Windows won't allow two sockets on a port
};

#elif defined(__APPLE__) || defined(__FreeBSD__)

// BSD derivatives don't support the same SO_REUSEADDR semantics as Linux so
// do the same thing as windows and hope for the best
extern "C" {
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <unistd.h>
# include <netdb.h>
}

void check_err(int ret, const std::string& what) {
    if (ret) throw std::runtime_error(what + ": " + std::strerror(errno));
}

class test_socket {
  public:
    int sock_;
    test_socket() : sock_(socket(AF_INET, SOCK_STREAM, 0)) {
        check_err(sock_ < 0, "socket");
        int on = 1;
        check_err(setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on)),
                  "setsockop");
    }
    ~test_socket() { }
    void close_early() {
        close(sock_);
    }
};

#else  /* POSIX */

extern "C" {
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <unistd.h>
# include <netdb.h>
}

void check_err(int ret, const std::string& what) {
    if (ret) throw std::runtime_error(what + ": " + std::strerror(errno));
}

class test_socket {
  public:
    int sock_;
    test_socket() : sock_(socket(AF_INET, SOCK_STREAM, 0)) {
        check_err(sock_ < 0, "socket");
        int on = 1;
        check_err(setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on)),
                  "setsockop");
    }
    ~test_socket() { close(sock_); }
    void close_early() {}   // Don't close early on POSIX, keep the port safe
};

#endif

#define TEST_PORT_MAX_STR 1060

/* Acquire a port suitable for listening */
class test_port {
    test_socket sock_;
    int port_;

  public:

    /* Acquire a port suitable for listening */
    test_port() : port_(0) {
        /* Create a socket and bind(INADDR_LOOPBACK:0) to get a free port.
           Set socket options so the port can be bound and used for listen() within this process,
           even though it is bound to the test_port socket.
           Use host to create the host_port address string.
        */
        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;    /* set the type of connection to TCP/IP */
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;            /* bind to port 0 */
        check_err(bind(sock_.sock_, (struct sockaddr*)&addr, sizeof(addr)), "bind");
        socklen_t len = sizeof(addr);
        check_err(getsockname(sock_.sock_, (struct sockaddr*)&addr, &len), "getsockname");
        port_ = ntohs(addr.sin_port);
        sock_.close_early();
    }

    int port() const { return port_; }

    std::string url(const std::string& host="") const {
        std::ostringstream url;
        url << "amp://" << host << ":" << port_;
        return url.str();
    }
};



#endif // TEST_PORT_HPP
