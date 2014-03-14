/*
 *
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
 *
 */

#include <proton/io.h>
#include <proton/object.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

#include "../platform.h"

#define MAX_HOST (1024)
#define MAX_SERV (64)

struct pn_io_t {
  char host[MAX_HOST];
  char serv[MAX_SERV];
  pn_error_t *error;
  bool wouldblock;
};

void pn_io_initialize(void *obj)
{
  pn_io_t *io = (pn_io_t *) obj;
  io->error = pn_error();
  io->wouldblock = false;
}

void pn_io_finalize(void *obj)
{
  pn_io_t *io = (pn_io_t *) obj;
  pn_error_free(io->error);
}

#define pn_io_hashcode NULL
#define pn_io_compare NULL
#define pn_io_inspect

pn_io_t *pn_io(void)
{
  static pn_class_t clazz = PN_CLASS(pn_io);
  pn_io_t *io = (pn_io_t *) pn_new(sizeof(pn_io_t), &clazz);
  return io;
}

void pn_io_free(pn_io_t *io)
{
  pn_free(io);
}

pn_error_t *pn_io_error(pn_io_t *io)
{
  assert(io);
  return io->error;
}

int pn_pipe(pn_io_t *io, pn_socket_t *dest)
{
  int n = pipe(dest);
  if (n) {
    pn_i_error_from_errno(io->error, "pipe");
  }

  return n;
}

static void pn_configure_sock(pn_io_t *io, pn_socket_t sock) {
  // this would be nice, but doesn't appear to exist on linux
  /*
  int set = 1;
  if (!setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int))) {
    pn_i_error_from_errno(io->error, "setsockopt");
  };
  */

  int flags = fcntl(sock, F_GETFL);
  flags |= O_NONBLOCK;

  if (fcntl(sock, F_SETFL, flags) < 0) {
    pn_i_error_from_errno(io->error, "fcntl");
  }

  //
  // Disable the Nagle algorithm on TCP connections.
  //
  // Note:  It would be more correct for the "level" argument to be SOL_TCP.  However, there
  //        are portability issues with this macro so we use IPPROTO_TCP instead.
  //
  int tcp_nodelay = 1;
  if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void*) &tcp_nodelay, sizeof(tcp_nodelay)) < 0) {
    pn_i_error_from_errno(io->error, "setsockopt");
  }
}

static inline int pn_create_socket(void);

pn_socket_t pn_listen(pn_io_t *io, const char *host, const char *port)
{
  struct addrinfo *addr;
  int code = getaddrinfo(host, port, NULL, &addr);
  if (code) {
    pn_error_format(io->error, PN_ERR, "getaddrinfo(%s, %s): %s\n", host, port, gai_strerror(code));
    return PN_INVALID_SOCKET;
  }

  pn_socket_t sock = pn_create_socket();
  if (sock == PN_INVALID_SOCKET) {
    pn_i_error_from_errno(io->error, "pn_create_socket");
    return PN_INVALID_SOCKET;
  }

  int optval = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
    pn_i_error_from_errno(io->error, "setsockopt");
    close(sock);
    return PN_INVALID_SOCKET;
  }

  if (bind(sock, addr->ai_addr, addr->ai_addrlen) == -1) {
    pn_i_error_from_errno(io->error, "bind");
    freeaddrinfo(addr);
    close(sock);
    return PN_INVALID_SOCKET;
  }

  freeaddrinfo(addr);

  if (listen(sock, 50) == -1) {
    pn_i_error_from_errno(io->error, "listen");
    close(sock);
    return PN_INVALID_SOCKET;
  }

  return sock;
}

pn_socket_t pn_connect(pn_io_t *io, const char *host, const char *port)
{
  struct addrinfo *addr;
  int code = getaddrinfo(host, port, NULL, &addr);
  if (code) {
    pn_error_format(io->error, PN_ERR, "getaddrinfo(%s, %s): %s", host, port, gai_strerror(code));
    return PN_INVALID_SOCKET;
  }

  pn_socket_t sock = pn_create_socket();
  if (sock == PN_INVALID_SOCKET) {
    pn_i_error_from_errno(io->error, "pn_create_socket");
    return PN_INVALID_SOCKET;
  }

  pn_configure_sock(io, sock);

  if (connect(sock, addr->ai_addr, addr->ai_addrlen) == -1) {
    if (errno != EINPROGRESS) {
      pn_i_error_from_errno(io->error, "connect");
      freeaddrinfo(addr);
      close(sock);
      return PN_INVALID_SOCKET;
    }
  }

  freeaddrinfo(addr);

  return sock;
}

pn_socket_t pn_accept(pn_io_t *io, pn_socket_t socket, char *name, size_t size)
{
  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  socklen_t addrlen = sizeof(addr);
  pn_socket_t sock = accept(socket, (struct sockaddr *) &addr, &addrlen);
  if (sock == PN_INVALID_SOCKET) {
    pn_i_error_from_errno(io->error, "accept");
    return sock;
  } else {
    int code;
    if ((code = getnameinfo((struct sockaddr *) &addr, addrlen, io->host, MAX_HOST, io->serv, MAX_SERV, 0))) {
      pn_error_format(io->error, PN_ERR, "getnameinfo: %s\n", gai_strerror(code));
      if (close(sock) == -1)
        pn_i_error_from_errno(io->error, "close");
      return PN_INVALID_SOCKET;
    } else {
      pn_configure_sock(io, sock);
      snprintf(name, size, "%s:%s", io->host, io->serv);
      return sock;
    }
  }
}

/* Abstract away turning off SIGPIPE */
#ifdef MSG_NOSIGNAL
ssize_t pn_send(pn_io_t *io, pn_socket_t socket, const void *buf, size_t len) {
  ssize_t count = send(socket, buf, len, MSG_NOSIGNAL);
  io->wouldblock = count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK);
  return count;
}

static inline int pn_create_socket(void) {
  return socket(AF_INET, SOCK_STREAM, getprotobyname("tcp")->p_proto);
}
#elif defined(SO_NOSIGPIPE)
ssize_t pn_send(pn_io_t *io, pn_socket_t socket, const void *buf, size_t size) {
  ssize_t count = return send(socket, buf, len, 0);
  io->wouldblock = count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK);
  return count;
}

static inline int pn_create_socket(void) {
  int sock = socket(AF_INET, SOCK_STREAM, getprotobyname("tcp")->p_proto);
  if (sock == -1) return sock;

  int optval = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval)) == -1) {
    close(sock);
    return -1;
  }
  return sock;
}
#else
#error "Don't know how to turn off SIGPIPE on this platform"
#endif

ssize_t pn_recv(pn_io_t *io, pn_socket_t socket, void *buf, size_t size)
{
  ssize_t count = recv(socket, buf, size, 0);
  io->wouldblock = count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK);
  return count;
}

ssize_t pn_write(pn_io_t *io, pn_socket_t socket, const void *buf, size_t size)
{
  return write(socket, buf, size);
}

ssize_t pn_read(pn_io_t *io, pn_socket_t socket, void *buf, size_t size)
{
  return read(socket, buf, size);
}

void pn_close(pn_io_t *io, pn_socket_t socket)
{
  close(socket);
}

bool pn_wouldblock(pn_io_t *io)
{
  return io->wouldblock;
}
