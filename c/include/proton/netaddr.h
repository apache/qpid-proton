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
 * Format a network address string in `buf`.
 *
 * @return the length of the string (excluding trailing '\0'), if >= size then
 * the address was truncated.
 */
PNP_EXTERN int pn_netaddr_str(const pn_netaddr_t *addr, char *buf, size_t size);

/**
 * Get the local address of a transport. Return `NULL` if not available.
 * Pointer is invalid after the transport closes (PN_TRANSPORT_CLOSED event is handled)
 */
PNP_EXTERN const pn_netaddr_t *pn_transport_local_addr(pn_transport_t *t);

/**
 * Get the local address of a transport. Return `NULL` if not available.
 * Pointer is invalid after the transport closes (PN_TRANSPORT_CLOSED event is handled)
 */
PNP_EXTERN const pn_netaddr_t *pn_transport_remote_addr(pn_transport_t *t);

/**
 * Get the listening addresses of a listener.
 * Addresses are only available after the @ref PN_LISTENER_OPEN event for the listener.
 *
 * A listener can have more than one address for several reasons:
 * - DNS host records may indicate more than one address
 * - On a multi-homed host, listening on the default host "" will listen on all local addresses.
 * - Some IPv4/IPV6 configurations may expand a single address into a v4/v6 pair.
 *
 * pn_netaddr_next() will iterate over the addresses in the list.
 *
 * @param l points to the listener
 * @return The first listening address or NULL if there are no addresses are available.
 * Use pn_netaddr_next() to iterate over the list.
 * Pointer is invalid after the listener closes (PN_LISTENER_CLOSED event is handled)
 */
PNP_EXTERN const pn_netaddr_t *pn_listener_addr(pn_listener_t *l);

/**
 * @return Pointer to the next address in a list of addresses, NULL if at the end of the list or
 * if this address is not part of a list.
 */
PNP_EXTERN const pn_netaddr_t *pn_netaddr_next(const pn_netaddr_t *na);

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
 * Get the host and port name from na as separate strings.
 * Returns 0 if successful, non-0 on error.
 */
PNP_EXTERN int pn_netaddr_host_port(const pn_netaddr_t* na, char *host, size_t hlen, char *port, size_t plen);

/**
 * **Deprecated** - Use ::pn_transport_local_addr()
 */
PN_DEPRECATED("Use pn_transport_local_addr")
PNP_EXTERN const pn_netaddr_t *pn_netaddr_local(pn_transport_t *t);

/**
 * **Deprecated** - Use ::pn_transport_remote_addr()
 */
PN_DEPRECATED("Use pn_transport_remote_addr")
PNP_EXTERN const pn_netaddr_t *pn_netaddr_remote(pn_transport_t *t);

/**
 * **Deprecated** - Use ::pn_listener_addr()
 */
PN_DEPRECATED("Use pn_listener_addr")
PNP_EXTERN const pn_netaddr_t *pn_netaddr_listening(pn_listener_t *l);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* PROTON_NETADDR_H */
