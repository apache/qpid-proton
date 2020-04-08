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

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#if _WIN32_WINNT < 0x0501
#error "Proton requires Windows API support for XP or later."
#endif
#include <winsock2.h>
#include <Ws2tcpip.h>
#else
/* Enable POSIX features beyond c99 for modern pthread and standard strerror_r() */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
/* Avoid GNU extensions, in particular the incompatible alternative strerror_r() */
#undef _GNU_SOURCE
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

#include "netaddr-internal.h"

#include <proton/netaddr.h>
#include <proton/proactor.h>

/* Common code for proactors that use the POSIX/Winsock sockaddr library for socket addresses. */

const struct sockaddr *pn_netaddr_sockaddr(const pn_netaddr_t *na) {
  return na ? (struct sockaddr*)&na->ss : NULL;
}

size_t pn_netaddr_socklen(const pn_netaddr_t *na) {
  if (!na) return 0;
  switch (na->ss.sa.sa_family) {
   case AF_INET: return sizeof(struct sockaddr_in);
   case AF_INET6: return sizeof(struct sockaddr_in6);
   default: return sizeof(na->ss);
  }
}

const pn_netaddr_t *pn_netaddr_next(const pn_netaddr_t *na) {
  return na ? na->next : NULL;
}

#ifndef NI_MAXHOST
# define NI_MAXHOST 1025
#endif

#ifndef NI_MAXSERV
# define NI_MAXSERV 32
#endif

int pn_netaddr_host_port(const pn_netaddr_t* na, char *host, size_t hlen, char *port, size_t plen) {
  return getnameinfo(pn_netaddr_sockaddr(na), pn_netaddr_socklen(na),
                     host, hlen, port, plen, NI_NUMERICHOST | NI_NUMERICSERV);
}

int pn_netaddr_str(const pn_netaddr_t* na, char *buf, size_t len) {
  char host[NI_MAXHOST];
  char port[NI_MAXSERV];
  int err = pn_netaddr_host_port(na, host, sizeof(host), port, sizeof(port));
  if (!err) {
    return pn_proactor_addr(buf, len, host, port);
  } else {
    if (buf) *buf = '\0';
    return 0;
  }
}
