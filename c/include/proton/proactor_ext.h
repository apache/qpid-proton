#ifndef PROTON_PROACTOR_EXT_H
#define PROTON_PROACTOR_EXT_H 1

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

#ifdef _WIN32
#include <stdint.h>
typedef uintptr_t pn_socket_t;
#else
typedef int pn_socket_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * @brief Extended proactor functions for native or implementation-specific integration.
 */

/**
 * Import an existing connected socket into the @p proactor and bind to a @p connection and @p transport.
 * Errors are returned as  @ref PN_TRANSPORT_CLOSED events by pn_proactor_wait().
 *
 * The @p proactor *takes ownership* of @p connection and will
 * automatically call pn_connection_free() after the final @ref
 * PN_TRANSPORT_CLOSED event is handled, or when pn_proactor_free() is
 * called. You can prevent the automatic free with
 * pn_proactor_release_connection()
 *
 * The @p proactor *takes ownership* of @p transport, it will be freed even
 * if pn_proactor_release_connection() is called.
 *
 * The @p proactor takes ownership of the socket and will close it when the connection is closed.
 *
 * @note Thread-safe
 *
 * @param[in] proactor   The proactor object.
 * @param[in] connection The connection object, or NULL to create a new one.
 * @param[in] transport  The transport object, or NULL to create a new one.
 * @param[in] fd         The file descriptor or socket handle of the connected socket.
 */
PNP_EXTERN void pn_proactor_import_socket(pn_proactor_t *proactor, pn_connection_t *connection, pn_transport_t *transport, pn_socket_t fd);

#ifdef __cplusplus
}
#endif

#endif /* PROTON_PROACTOR_EXT_H */
