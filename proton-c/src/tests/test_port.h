#ifndef TESTS_TEST_PORT_H
#define TESTS_TEST_PORT_H

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

/* Some simple platform-secifics to acquire an unused socket */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)

# include <winsock2.h>
# include <ws2tcpip.h>

typedef SOCKET sock_t;

static void test_snprintf(char *buf, size_t count, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  _vsnprintf(buf, count, fmt, ap);
  buf[count-1] = '\0';          /* _vsnprintf doesn't null-terminate on overflow */
}

void check_err(int ret, const char *what) {
  if (ret) {
    char buf[512];
    FormatMessage(
      FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL, WSAGetLastError(), NULL, buf, sizeof(buf), NULL);
    fprintf(stderr, "%s: %s\n", what, buf);
    abort();
  }
}

#else  /* POSIX */

# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <unistd.h>
# include <netdb.h>
# define  test_snprintf snprintf

typedef int sock_t;

void check_err(int ret, const char *what) {
  if (ret) {
    perror(what); abort();
  }
}

#endif

#define TEST_PORT_MAX_STR 1060

/* Combines a sock_t with the int and char* versions of the port for convenience */
typedef struct test_port_t {
  sock_t sock;
  int port;                     /* port as integer */
  char str[TEST_PORT_MAX_STR];	/* port as string */
  char host_port[TEST_PORT_MAX_STR]; /* host:port string */
} test_port_t;

/* Modifies tp->host_port to use host, returns the new tp->host_port */
const char *test_port_use_host(test_port_t *tp, const char *host) {
  test_snprintf(tp->host_port, sizeof(tp->host_port), "%s:%d", host, tp->port);
  return tp->host_port;
}

/* Create a socket and bind(INADDR_LOOPBACK:0) to get a free port.
   Set socket options so the port can be bound and used for listen() within this process,
   even though it is bound to the test_port socket.
   Use host to create the host_port address string.
*/
test_port_t test_port(const char* host) {
#ifdef _WIN32
  static int wsa_started = 0;
  if (!wsa_started) {
    WORD wsa_ver = MAKEWORD(2, 2);
    WSADATA unused;
    check_err(WSAStartup(wsa_ver, &unused), "WSAStartup");
  }
#endif
  int err = 0;
  test_port_t tp = {0};
  tp.sock = socket(AF_INET, SOCK_STREAM, 0);
  check_err(tp.sock < 0, "socket");
#ifndef _WIN32
  int on = 1;
  check_err(setsockopt(tp.sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on)), "setsockopt");;;
#endif
  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;    /* set the type of connection to TCP/IP */
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;            /* bind to port 0 */
  err = bind(tp.sock, (struct sockaddr*)&addr, sizeof(addr));
  check_err(err, "bind");
  socklen_t len = sizeof(addr);
  err = getsockname(tp.sock, (struct sockaddr*)&addr, &len); /* Get the bound port */
  check_err(err, "getsockname");
  tp.port = ntohs(addr.sin_port);
  test_snprintf(tp.str, sizeof(tp.str), "%d", tp.port);
  test_port_use_host(&tp, host);
#ifdef _WIN32                   /* Windows doesn't support the twice-open socket trick */
  closesocket(tp.sock);
#endif
  return tp;
}

void test_port_close(test_port_t *tp) {
#ifdef _WIN32
  WSACleanup();
#else
  close(tp->sock);
#endif
}


#endif // TESTS_TEST_PORT_H
