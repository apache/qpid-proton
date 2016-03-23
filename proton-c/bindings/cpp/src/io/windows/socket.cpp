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

#include "msg.hpp"

#include <proton/io/socket.hpp>
#include <proton/url.hpp>

#define FD_SETSIZE 2048
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#if _WIN32_WINNT < 0x0501
#error "Proton requires Windows API support for XP or later."
#endif
#include <winsock2.h>
#include <mswsock.h>
#include <Ws2tcpip.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>

namespace proton {
namespace io {
namespace socket {

const descriptor INVALID_DESCRIPTOR = INVALID_SOCKET;

std::string error_str() {
    HRESULT code = WSAGetLastError();
    char err[1024] = {0};
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
                  FORMAT_MESSAGE_MAX_WIDTH_MASK, NULL, code, 0, (LPSTR)&err, sizeof(err), NULL);
    return err;
}

io_error::io_error(const std::string& s) : error(s) {}

namespace {

template <class T> T check(T result, const std::string& msg=std::string()) {
    if (result == SOCKET_ERROR)
        throw io_error(msg + error_str());
    return result;
}

void gai_check(int result, const std::string& msg="") {
    if (result)
        throw io_error(msg + gai_strerror(result));
}

} // namespace

void initialize() {
    WSADATA unused;
    check(WSAStartup(0x0202, &unused), "can't load WinSock: "); // Version 2.2
}

void finalize() {
    WSACleanup();
}

void engine::init() {
    u_long nonblock = 1;
    check(::ioctlsocket(socket_, FIONBIO, &nonblock), "ioctlsocket: ");
}

engine::engine(descriptor fd, handler& h, const connection_options &opts)
    : connection_engine(h, opts), socket_(fd)
{
    init();
}

engine::engine(const url& u, handler& h, const connection_options &opts)
    : connection_engine(h, opts), socket_(connect(u))
{
    init();
    connection().open();
}

engine::~engine() {}

void engine::read() {
    mutable_buffer rbuf = read_buffer();
    if (rbuf.size > 0) {
        int n = ::recv(socket_, rbuf.data, rbuf.size, 0);
        if (n > 0)
            read_done(n);
        else if (n == 0)
            read_close();
        else if (n == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
            close("io_error", error_str());
    }
}

void engine::write() {
    const_buffer wbuf = write_buffer();
    if (wbuf.size > 0) {
    int n = ::send(socket_, wbuf.data, wbuf.size, 0);
    if (n > 0)
        write_done(n);
    else if (n == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
        close("io_error", error_str());
    }
}

void engine::run() {
    while (dispatch()) {
        fd_set rd, wr;
        FD_ZERO(&rd);
        if (read_buffer().size)
            FD_SET(socket_, &rd);
        FD_ZERO(&wr);
        if (write_buffer().size)
            FD_SET(socket_, &wr);
        int n = ::select(FD_SETSIZE, &rd, &wr, NULL, NULL);
        if (n < 0) {
            close("io_error", error_str());
            break;
        }
        if (FD_ISSET(socket_, &rd)) {
            read();
        }
        if (FD_ISSET(socket_, &wr))
            write();
    }
    ::closesocket(socket_);
}

namespace {
struct auto_addrinfo {
    struct addrinfo *ptr;
    auto_addrinfo() : ptr(0) {}
    ~auto_addrinfo() { ::freeaddrinfo(ptr); }
    addrinfo* operator->() const { return ptr; }
};

static const char *amqp_service(const char *port) {
  // Help older Windows to know about amqp[s] ports
  if (port) {
    if (!strcmp("amqp", port)) return "5672";
    if (!strcmp("amqps", port)) return "5671";
  }
  return port;
}
}


descriptor connect(const proton::url& u) {
    // convert "0.0.0.0" to "127.0.0.1" on Windows for outgoing sockets
    std::string host = (u.host() == "0.0.0.0") ? "127.0.0.1" : u.host();
    descriptor fd = INVALID_SOCKET;
    try{
        auto_addrinfo addr;
        gai_check(::getaddrinfo(host.empty() ? 0 : host.c_str(),
                                amqp_service(u.port().empty() ? 0 : u.port().c_str()),
                                0, &addr.ptr),
                  "connect address invalid: ");
        fd = check(::socket(addr->ai_family, SOCK_STREAM, 0), "connect socket: ");
        check(::connect(fd, addr->ai_addr, addr->ai_addrlen), "connect: ");
        return fd;
    } catch (...) {
        if (fd != INVALID_SOCKET) ::closesocket(fd);
        throw;
    }
}

listener::listener(const std::string& host, const std::string &port) : socket_(INVALID_SOCKET) {
    try {
        auto_addrinfo addr;
        gai_check(::getaddrinfo(host.empty() ? 0 : host.c_str(),
                                port.empty() ? 0 : port.c_str(), 0, &addr.ptr),
                  "listener address invalid: ");
        socket_ = check(::socket(addr->ai_family, SOCK_STREAM, 0), "listener socket: ");
        bool yes = true;
        check(setsockopt(socket_, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&yes, sizeof(yes)), "setsockopt: ");
        check(::bind(socket_, addr->ai_addr, addr->ai_addrlen), "listener bind: ");
        check(::listen(socket_, 32), "listener listen: ");
    } catch (...) {
        if (socket_ != INVALID_SOCKET) ::closesocket(socket_);
        throw;
    }
}

listener::~listener() { ::closesocket(socket_); }

descriptor listener::accept(std::string& host_str, std::string& port_str) {
    struct sockaddr_storage addr;
    socklen_t size = sizeof(addr);
    int fd = check(::accept(socket_, (struct sockaddr *)&addr, &size), "accept: ");
    char host[NI_MAXHOST], port[NI_MAXSERV];
    gai_check(getnameinfo((struct sockaddr *) &addr, sizeof(addr),
                          host, sizeof(host), port, sizeof(port), 0),
              "accept invalid remote address: ");
    host_str = host;
    port_str = port;
    return fd;
}

}}}
