#ifndef PROTON_NETADDR_H
#define PROTON_NETADDR_H

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

#include <proton/import_export.h>
#include <proton/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 *
 * @copybrief pn_netaddr_t
 *
 * @addtogroup proactor
 * @{
 */

/**
 * **Unsettled API** - The network address of a proactor transport.
 */
typedef struct pn_netaddr_t pn_netaddr_t;

/**
 * Format a network address as a human-readable string in `buf`.
 *
 * @return the length of the string (excluding trailing '\0'), if >= size then
 * the address was truncated.
 */
PNP_EXTERN int pn_netaddr_str(const pn_netaddr_t *addr, char *buf, size_t size);

/**
 * Get the local address of a transport. Return `NULL` if not available.
 */
PNP_EXTERN const pn_netaddr_t *pn_netaddr_local(pn_transport_t *t);

/**
 * Get the remote address of a transport. Return NULL if not available.
 */
PNP_EXTERN const pn_netaddr_t *pn_netaddr_remote(pn_transport_t *t);

struct sockaddr;

/**
 * On POSIX or Windows, get the underlying `struct sockaddr`.
 * Return NULL if not available.
 */
PNP_EXTERN const struct sockaddr *pn_netaddr_sockaddr(const pn_netaddr_t *na);

/**
 * On POSIX or Windows, get the size of the underlying `struct sockaddr`.
 * Return 0 if not available.
 */
PNP_EXTERN size_t pn_netaddr_socklen(const pn_netaddr_t *na);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* PROTON_NETADDR_H */
