#ifndef PROTON_IO_H
#define PROTON_IO_H 1

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

#include <proton/import_export.h>
#include <proton/error.h>
#include <sys/types.h>
#include <proton/type_compat.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) && ! defined(__CYGWIN__)
#ifdef _WIN64
typedef unsigned __int64 pn_socket_t;
#else
typedef unsigned int pn_socket_t;
#endif
#define PN_INVALID_SOCKET (pn_socket_t)(~0)
#else
typedef int pn_socket_t;
#define PN_INVALID_SOCKET (-1)
#endif

typedef struct pn_io_t pn_io_t;
typedef struct pn_selector_t pn_selector_t;

PN_EXTERN pn_io_t *pn_io(void);
PN_EXTERN void pn_io_free(pn_io_t *io);
PN_EXTERN pn_error_t *pn_io_error(pn_io_t *io);
PN_EXTERN pn_socket_t pn_connect(pn_io_t *io, const char *host, const char *port);
PN_EXTERN pn_socket_t pn_listen(pn_io_t *io, const char *host, const char *port);
PN_EXTERN pn_socket_t pn_accept(pn_io_t *io, pn_socket_t socket, char *name, size_t size);
PN_EXTERN void pn_close(pn_io_t *io, pn_socket_t socket);
PN_EXTERN ssize_t pn_send(pn_io_t *io, pn_socket_t socket, const void *buf, size_t size);
PN_EXTERN ssize_t pn_recv(pn_io_t *io, pn_socket_t socket, void *buf, size_t size);
PN_EXTERN int pn_pipe(pn_io_t *io, pn_socket_t *dest);
PN_EXTERN ssize_t pn_read(pn_io_t *io, pn_socket_t socket, void *buf, size_t size);
PN_EXTERN ssize_t pn_write(pn_io_t *io, pn_socket_t socket, const void *buf, size_t size);
PN_EXTERN bool pn_wouldblock(pn_io_t *io);
PN_EXTERN pn_selector_t *pn_io_selector(pn_io_t *io);

#ifdef __cplusplus
}
#endif

#endif /* io.h */
