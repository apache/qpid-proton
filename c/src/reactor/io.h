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

#include "selector.h"

#include <proton/import_export.h>
#include <proton/error.h>
#include <proton/type_compat.h>
#include <stddef.h>

/**
 * A ::pn_io_t manages IO for a group of pn_socket_t handles.  A
 * pn_io_t object may have zero or one pn_selector_t selectors
 * associated with it (see ::pn_io_selector()).  If one is associated,
 * all the pn_socket_t handles managed by a pn_io_t must use that
 * pn_selector_t instance.
 *
 * The pn_io_t interface is single-threaded. All methods are intended
 * to be used by one thread at a time, except that multiple threads
 * may use:
 *
 *   ::pn_write()
 *   ::pn_send()
 *   ::pn_recv()
 *   ::pn_close()
 *   ::pn_selector_select()
 *
 * provided at most one thread is calling ::pn_selector_select() and
 * the other threads are operating on separate pn_socket_t handles.
 */
typedef struct pn_io_t pn_io_t;

pn_io_t *pn_io(void);
void pn_io_free(pn_io_t *io);
pn_error_t *pn_io_error(pn_io_t *io);
pn_socket_t pn_connect(pn_io_t *io, const char *host, const char *port);
pn_socket_t pn_listen(pn_io_t *io, const char *host, const char *port);

pn_socket_t pn_accept(pn_io_t *io, pn_socket_t socket, char *name, size_t size);
void pn_close(pn_io_t *io, pn_socket_t socket);
ssize_t pn_send(pn_io_t *io, pn_socket_t socket, const void *buf, size_t size);
ssize_t pn_recv(pn_io_t *io, pn_socket_t socket, void *buf, size_t size);
int pn_pipe(pn_io_t *io, pn_socket_t *dest);
ssize_t pn_read(pn_io_t *io, pn_socket_t socket, void *buf, size_t size);
ssize_t pn_write(pn_io_t *io, pn_socket_t socket, const void *buf, size_t size);
bool pn_wouldblock(pn_io_t *io);
pn_selector_t *pn_io_selector(pn_io_t *io);

#endif /* io.h */
