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
#define PN_WINAPI

#include "platform.h"
#include <proton/io.h>
#include <proton/object.h>
#include <proton/selector.h>
#include "iocp.h"
#include "util.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>

int pni_win32_error(pn_error_t *error, const char *msg, HRESULT code)
{
  // Error code can be from GetLastError or WSAGetLastError,
  char err[1024] = {0};
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
                FORMAT_MESSAGE_MAX_WIDTH_MASK, NULL, code, 0, (LPSTR)&err, sizeof(err), NULL);
  return pn_error_format(error, PN_ERR, "%s: %s", msg, err);
}

static void io_log(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fflush(stderr);
}

struct pn_io_t {
  char host[NI_MAXHOST];
  char serv[NI_MAXSERV];
  pn_error_t *error;
  bool trace;
  bool wouldblock;
  iocp_t *iocp;
};

void pn_io_initialize(void *obj)
{
  pn_io_t *io = (pn_io_t *) obj;
  io->error = pn_error();
  io->wouldblock = false;
  io->trace = pn_env_bool("PN_TRACE_DRV");

  /* Request WinSock 2.2 */
  WORD wsa_ver = MAKEWORD(2, 2);
  WSADATA unused;
  int err = WSAStartup(wsa_ver, &unused);
  if (err) {
    pni_win32_error(io->error, "WSAStartup", WSAGetLastError());
    fprintf(stderr, "Can't load WinSock: %d\n", pn_error_text(io->error));
  }
  io->iocp = pni_iocp();
}

void pn_io_finalize(void *obj)
{
  pn_io_t *io = (pn_io_t *) obj;
  pn_error_free(io->error);
  pn_free(io->iocp);
  WSACleanup();
}

#define pn_io_hashcode NULL
#define pn_io_compare NULL
#define pn_io_inspect

pn_io_t *pn_io(void)
{
  static const pn_class_t clazz = PN_CLASS(pn_io);
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

static void ensure_unique(pn_io_t *io, pn_socket_t new_socket)
{
  // A brand new socket can have the same HANDLE value as a previous
  // one after a socketclose.  If the application closes one itself
  // (i.e. not using pn_close), we don't find out about it until here.
  iocpdesc_t *iocpd = pni_iocpdesc_map_get(io->iocp, new_socket);
  if (iocpd) {
    if (io->trace)
      io_log("Stale external socket reference discarded\n");
    // Re-use means former socket instance was closed
    assert(iocpd->ops_in_progress == 0);
    assert(iocpd->external);
    // Clean up the straggler as best we can
    pn_socket_t sock = iocpd->socket;
    iocpd->socket = INVALID_SOCKET;
    pni_iocpdesc_map_del(io->iocp, sock);  // may free the iocpdesc_t depending on refcount
  }
}


/*
 * This heavyweight surrogate pipe could be replaced with a normal Windows pipe
 * now that select() is no longer used.  If interrupt semantics are all that is
 * needed, a simple user space counter and reserved completion status would
 * probably suffice.
 */
static int pni_socket_pair(pn_io_t *io, SOCKET sv[2]);

int pn_pipe(pn_io_t *io, pn_socket_t *dest)
{
  int n = pni_socket_pair(io, dest);
  if (n) {
    pni_win32_error(io->error, "pipe", WSAGetLastError());
  }
  return n;
}

static void pn_configure_sock(pn_io_t *io, pn_socket_t sock) {
  //
  // Disable the Nagle algorithm on TCP connections.
  //
  int flag = 1;
  if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag)) != 0) {
    perror("setsockopt");
  }

  u_long nonblock = 1;
  if (ioctlsocket(sock, FIONBIO, &nonblock)) {
    perror("ioctlsocket");
  }
}

static inline pn_socket_t pni_create_socket();

pn_socket_t pn_listen(pn_io_t *io, const char *host, const char *port)
{
  struct addrinfo *addr;
  int code = getaddrinfo(host, port, NULL, &addr);
  if (code) {
    pn_error_format(io->error, PN_ERR, "getaddrinfo(%s, %s): %s\n", host, port, gai_strerror(code));
    return INVALID_SOCKET;
  }

  pn_socket_t sock = pni_create_socket();
  if (sock == INVALID_SOCKET) {
    pni_win32_error(io->error, "pni_create_socket", WSAGetLastError());
    return INVALID_SOCKET;
  }
  ensure_unique(io, sock);

  bool optval = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char *) &optval,
                 sizeof(optval)) == -1) {
    pni_win32_error(io->error, "setsockopt", WSAGetLastError());
    closesocket(sock);
    return INVALID_SOCKET;
  }

  if (bind(sock, addr->ai_addr, addr->ai_addrlen) == -1) {
    pni_win32_error(io->error, "bind", WSAGetLastError());
    freeaddrinfo(addr);
    closesocket(sock);
    return INVALID_SOCKET;
  }
  freeaddrinfo(addr);

  if (listen(sock, 50) == -1) {
    pni_win32_error(io->error, "listen", WSAGetLastError());
    closesocket(sock);
    return INVALID_SOCKET;
  }

  iocpdesc_t *iocpd = pni_iocpdesc_create(io->iocp, sock, false);
  if (!iocpd) {
    pn_i_error_from_errno(io->error, "register");
    closesocket(sock);
    return INVALID_SOCKET;
  }

  pni_iocpdesc_start(iocpd);
  return sock;
}

pn_socket_t pn_connect(pn_io_t *io, const char *hostarg, const char *port)
{
  // convert "0.0.0.0" to "127.0.0.1" on Windows for outgoing sockets
  const char *host = strcmp("0.0.0.0", hostarg) ? hostarg : "127.0.0.1";

  struct addrinfo *addr;
  int code = getaddrinfo(host, port, NULL, &addr);
  if (code) {
    pn_error_format(io->error, PN_ERR, "getaddrinfo(%s, %s): %s", host, port, gai_strerror(code));
    return INVALID_SOCKET;
  }

  pn_socket_t sock = pni_create_socket();
  if (sock == INVALID_SOCKET) {
    pni_win32_error(io->error, "proton pni_create_socket", WSAGetLastError());
    freeaddrinfo(addr);
    return INVALID_SOCKET;
  }

  ensure_unique(io, sock);
  pn_configure_sock(io, sock);
  return pni_iocp_begin_connect(io->iocp, sock, addr, io->error);
}

pn_socket_t pn_accept(pn_io_t *io, pn_socket_t listen_sock, char *name, size_t size)
{
  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  socklen_t addrlen = sizeof(addr);
  iocpdesc_t *listend = pni_iocpdesc_map_get(io->iocp, listen_sock);
  pn_socket_t accept_sock;

  if (listend)
    accept_sock = pni_iocp_end_accept(listend, (struct sockaddr *) &addr, &addrlen, &io->wouldblock, io->error);
  else {
    // User supplied socket
    accept_sock = accept(listen_sock, (struct sockaddr *) &addr, &addrlen);
    if (accept_sock == INVALID_SOCKET)
      pni_win32_error(io->error, "sync accept", WSAGetLastError());
  }

  if (accept_sock == INVALID_SOCKET)
    return accept_sock;

  int code = getnameinfo((struct sockaddr *) &addr, addrlen, io->host, NI_MAXHOST,
                         io->serv, NI_MAXSERV, 0);
  if (code)
    code = getnameinfo((struct sockaddr *) &addr, addrlen, io->host, NI_MAXHOST,
                       io->serv, NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV);
  if (code) {
    pn_error_format(io->error, PN_ERR, "getnameinfo: %s\n", gai_strerror(code));
    pn_close(io, accept_sock);
    return INVALID_SOCKET;
  } else {
    pn_configure_sock(io, accept_sock);
    snprintf(name, size, "%s:%s", io->host, io->serv);
    if (listend) {
      pni_iocpdesc_start(pni_iocpdesc_map_get(io->iocp, accept_sock));
    }
    return accept_sock;
  }
}

static inline pn_socket_t pni_create_socket() {
  return socket(AF_INET, SOCK_STREAM, getprotobyname("tcp")->p_proto);
}

ssize_t pn_send(pn_io_t *io, pn_socket_t sockfd, const void *buf, size_t len) {
  ssize_t count;
  iocpdesc_t *iocpd = pni_iocpdesc_map_get(io->iocp, sockfd);
  if (iocpd) {
    count = pni_iocp_begin_write(iocpd, buf, len, &io->wouldblock, io->error);
  } else {
    count = send(sockfd, (const char *) buf, len, 0);
    io->wouldblock = count < 0 && WSAGetLastError() == WSAEWOULDBLOCK;
  }
  return count;
}

ssize_t pn_recv(pn_io_t *io, pn_socket_t socket, void *buf, size_t size)
{
  ssize_t count;
  iocpdesc_t *iocpd = pni_iocpdesc_map_get(io->iocp, socket);
  if (iocpd) {
    count = pni_iocp_recv(iocpd, buf, size, &io->wouldblock, io->error);
  } else {
    count = recv(socket, (char *) buf, size, 0);
    io->wouldblock = count < 0 && WSAGetLastError() == WSAEWOULDBLOCK;
  }
  return count;
}

ssize_t pn_write(pn_io_t *io, pn_socket_t socket, const void *buf, size_t size)
{
  // non-socket io is mapped to socket io for now.  See pn_pipe()
  return pn_send(io, socket, buf, size);
}

ssize_t pn_read(pn_io_t *io, pn_socket_t socket, void *buf, size_t size)
{
  return pn_recv(io, socket, buf, size);
}

void pn_close(pn_io_t *io, pn_socket_t socket)
{
  iocpdesc_t *iocpd = pni_iocpdesc_map_get(io->iocp, socket);
  if (iocpd)
    pni_iocp_begin_close(iocpd);
  else {
    closesocket(socket);
  }
}

bool pn_wouldblock(pn_io_t *io)
{
  return io->wouldblock;
}

pn_selector_t *pn_io_selector(pn_io_t *io)
{
  if (io->iocp->selector == NULL)
    io->iocp->selector = pni_selector_create(io->iocp);
  return io->iocp->selector;
}

static void configure_pipe_socket(pn_io_t *io, pn_socket_t sock)
{
  u_long v = 1;
  ioctlsocket (sock, FIONBIO, &v);
  ensure_unique(io, sock);
  iocpdesc_t *iocpd = pni_iocpdesc_create(io->iocp, sock, false);
  pni_iocpdesc_start(iocpd);
}


static int pni_socket_pair (pn_io_t *io, SOCKET sv[2]) {
  // no socketpair on windows.  provide pipe() semantics using sockets

  SOCKET sock = socket(AF_INET, SOCK_STREAM, getprotobyname("tcp")->p_proto);
  if (sock == INVALID_SOCKET) {
    perror("socket");
    return -1;
  }

  BOOL b = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *) &b, sizeof(b)) == -1) {
    perror("setsockopt");
    closesocket(sock);
    return -1;
  }
  else {
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
      perror("bind");
      closesocket(sock);
      return -1;
    }
  }

  if (listen(sock, 50) == -1) {
    perror("listen");
    closesocket(sock);
    return -1;
  }

  if ((sv[1] = socket(AF_INET, SOCK_STREAM, getprotobyname("tcp")->p_proto)) == INVALID_SOCKET) {
    perror("sock1");
    closesocket(sock);
    return -1;
  }
  else {
    struct sockaddr addr = {0};
    int l = sizeof(addr);
    if (getsockname(sock, &addr, &l) == -1) {
      perror("getsockname");
      closesocket(sock);
      return -1;
    }

    if (connect(sv[1], &addr, sizeof(addr)) == -1) {
      int err = WSAGetLastError();
      fprintf(stderr, "connect wsaerrr %d\n", err);
      closesocket(sock);
      closesocket(sv[1]);
      return -1;
    }

    if ((sv[0] = accept(sock, &addr, &l)) == INVALID_SOCKET) {
      perror("accept");
      closesocket(sock);
      closesocket(sv[1]);
      return -1;
    }
  }

  configure_pipe_socket(io, sv[0]);
  configure_pipe_socket(io, sv[1]);
  closesocket(sock);
  return 0;
}

