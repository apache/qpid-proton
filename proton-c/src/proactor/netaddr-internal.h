#ifndef PROACTOR_NETADDR_INTERNAL_H
#define PROACTOR_NETADDR_INTERNAL_H

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

#include <proton/netaddr.h>

/* Common code for proactors that use the POSIX/Winsock sockaddr library for socket addresses. */

struct pn_netaddr_t {
  struct sockaddr_storage ss;
  pn_netaddr_t *next;
};

const struct sockaddr *pn_netaddr_sockaddr(const pn_netaddr_t *na) {
  return na ? (struct sockaddr*)&na->ss : NULL;
}

size_t pn_netaddr_socklen(const pn_netaddr_t *na) {
  return sizeof(na->ss);
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

/* Return port or -1 if sa is not a known address type */ 
static int get_port(const struct sockaddr *sa) {
  switch (sa->sa_family) {
   case AF_INET: return ((struct sockaddr_in*)sa)->sin_port;
   case AF_INET6: return ((struct sockaddr_in6*)sa)->sin6_port;
   default: return -1;
  }
}

/* Set the port in sa or do nothing if it is not a known address type */
static void set_port(struct sockaddr *sa, uint16_t port) {
  switch (sa->sa_family) {
   case AF_INET: ((struct sockaddr_in*)sa)->sin_port = port; break;
   case AF_INET6: ((struct sockaddr_in6*)sa)->sin6_port = port; break;
   default: break;
  }
}

/* If want has port=0 and got has port > 0 then return port of got, else return 0 */
static uint16_t check_dynamic_port(const struct sockaddr *want, const struct sockaddr *got) {
  if (get_port(want) == 0) {
    int port = get_port(got);
    if (port > 0) return (uint16_t)port;
  }
  return 0;
}

#endif  /*!PROACTOR_NETADDR_INTERNAL_H*/
