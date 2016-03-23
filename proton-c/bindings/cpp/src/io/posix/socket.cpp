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

#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>

namespace proton {
namespace io {
namespace socket {

io_error::io_error(const std::string& s) : error(s) {}

const descriptor INVALID_DESCRIPTOR = -1;

std::string error_str() {
    char buf[512] = "Unknown error";
#ifdef _GNU_SOURCE
    // GNU strerror_r returns the message
    return ::strerror_r(errno, buf, sizeof(buf));
#else
    // POSIX strerror_r doesn't return the buffer
    ::strerror_r(errno, buf, sizeof(buf));
    return std::string(buf)
#endif
}

namespace {

template <class T> T check(T result, const std::string& msg=std::string()) {
    if (result < 0) throw io_error(msg + error_str());
    return result;
}

void gai_check(int result, const std::string& msg="") {
    if (result) throw io_error(msg + gai_strerror(result));
}

}

void engine::init() {
    check(fcntl(socket_, F_SETFL, fcntl(socket_, F_GETFL, 0) | O_NONBLOCK), "set nonblock: ");
}

engine::engine(descriptor fd, handler& h, const connection_options &opts)
    : connection_engine(h, opts), socket_(fd)
{
    init();
}

engine::engine(const url& u, handler& h, const connection_options& opts)
    : connection_engine(h, opts), socket_(connect(u))
{
    init();
    connection().open();
}

engine::~engine() {}

void engine::read() {
    mutable_buffer rbuf = read_buffer();
    if (rbuf.size > 0) {
        ssize_t n = ::read(socket_, rbuf.data, rbuf.size);
        if (n > 0)
            read_done(n);
        else if (n == 0)
            read_close();
        else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
            close("io_error", error_str());
    }
}

void engine::write() {
    const_buffer wbuf = write_buffer();
    if (wbuf.size > 0) {
        ssize_t n = ::write(socket_, wbuf.data, wbuf.size);
        if (n > 0)
            write_done(n);
        else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            close("io_error", error_str());
        }
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
            close("select: ", error_str());
            break;
        }
        if (FD_ISSET(socket_, &rd)) {
            read();
        }
        if (FD_ISSET(socket_, &wr))
            write();
    }
    ::close(socket_);
}

namespace {
struct auto_addrinfo {
    struct addrinfo *ptr;
    auto_addrinfo() : ptr(0) {}
    ~auto_addrinfo() { ::freeaddrinfo(ptr); }
    addrinfo* operator->() const { return ptr; }
};
}

descriptor connect(const proton::url& u) {
    descriptor fd = INVALID_DESCRIPTOR;
    try{
        auto_addrinfo addr;
        gai_check(::getaddrinfo(u.host().empty() ? 0 : u.host().c_str(),
                                u.port().empty() ? 0 : u.port().c_str(),
                                0, &addr.ptr), u.str()+": ");
        fd = check(::socket(addr->ai_family, SOCK_STREAM, 0), "connect: ");
        check(::connect(fd, addr->ai_addr, addr->ai_addrlen), "connect: ");
        return fd;
    } catch (...) {
        if (fd >= 0) close(fd);
        throw;
    }
}

listener::listener(const std::string& host, const std::string &port) : socket_(INVALID_DESCRIPTOR) {
    try {
        auto_addrinfo addr;
        gai_check(::getaddrinfo(host.empty() ? 0 : host.c_str(),
                                port.empty() ? 0 : port.c_str(), 0, &addr.ptr),
                  "listener address invalid: ");
        socket_ = check(::socket(addr->ai_family, SOCK_STREAM, 0), "listen: ");
        int yes = 1;
        check(setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)), "setsockopt: ");
        check(::bind(socket_, addr->ai_addr, addr->ai_addrlen), "bind: ");
        check(::listen(socket_, 32), "listen: ");
    } catch (...) {
        if (socket_ >= 0) close(socket_);
        throw;
    }
}

listener::~listener() { ::close(socket_); }

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

// Empty stubs, only needed on windows.
void initialize() {}
void finalize() {}

}}}
