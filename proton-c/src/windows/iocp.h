#ifndef PROTON_SRC_IOCP_H
#define PROTON_SRC_IOCP_H 1

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
#include <proton/selectable.h>
#include <proton/type_compat.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pni_acceptor_t pni_acceptor_t;
typedef struct write_result_t write_result_t;
typedef struct read_result_t read_result_t;
typedef struct write_pipeline_t write_pipeline_t;
typedef struct iocpdesc_t iocpdesc_t;


// One per pn_io_t.

struct iocp_t {
  HANDLE completion_port;
  pn_hash_t *iocpdesc_map;
  pn_list_t *zombie_list;
  int shared_pool_size;
  char *shared_pool_memory;
  write_result_t **shared_results;
  write_result_t **available_results;
  size_t shared_available_count;
  size_t writer_count;
  int loopback_bufsize;
  bool iocp_trace;
  pn_selector_t *selector;
};


// One for each socket.
// This iocpdesc_t structure is ref counted by the iocpdesc_map, zombie_list,
// selector->iocp_descriptors list.  It should remain ref counted in the
// zombie_list until ops_in_progress == 0 or the completion port is closed.

struct iocpdesc_t {
  pn_socket_t socket;
  iocp_t *iocp;
  pni_acceptor_t *acceptor;
  pn_error_t *error;
  int ops_in_progress;
  bool read_in_progress;
  write_pipeline_t *pipeline;
  read_result_t *read_result;
  bool external;       // true if socket set up outside Proton
  bool bound;          // associted with the completion port
  bool closing;        // pn_close called
  bool read_closed;    // EOF or read error
  bool write_closed;   // shutdown sent or write error
  pn_selector_t *selector;
  pn_selectable_t *selectable;
  int events;
  int interests;
  pn_timestamp_t deadline;
  iocpdesc_t *triggered_list_next;
  iocpdesc_t *triggered_list_prev;
  iocpdesc_t *deadlines_next;
  iocpdesc_t *deadlines_prev;
  pn_timestamp_t reap_time;;
};

typedef enum { IOCP_ACCEPT, IOCP_CONNECT, IOCP_READ, IOCP_WRITE } iocp_type_t;

typedef struct {
  OVERLAPPED overlapped;
  iocp_type_t type;
  iocpdesc_t *iocpd;
  HRESULT status;
} iocp_result_t;

struct write_result_t {
  iocp_result_t base;
  size_t requested;
  bool in_use;
  pn_bytes_t buffer;
};

iocpdesc_t *pni_iocpdesc_create(iocp_t *, pn_socket_t s, bool external);
iocpdesc_t *pni_iocpdesc_map_get(iocp_t *, pn_socket_t s);
void pni_iocpdesc_map_del(iocp_t *, pn_socket_t s);
void pni_iocpdesc_map_push(iocpdesc_t *iocpd);
void pni_iocpdesc_start(iocpdesc_t *iocpd);
void pni_iocp_drain_completions(iocp_t *);
int pni_iocp_wait_one(iocp_t *, int timeout, pn_error_t *);
void pni_iocp_start_accepting(iocpdesc_t *iocpd);
pn_socket_t pni_iocp_end_accept(iocpdesc_t *ld, sockaddr *addr, socklen_t *addrlen, bool *would_block, pn_error_t *error);
pn_socket_t pni_iocp_begin_connect(iocp_t *, pn_socket_t sock, struct addrinfo *addr, pn_error_t *error);
ssize_t pni_iocp_begin_write(iocpdesc_t *, const void *, size_t, bool *, pn_error_t *);
ssize_t pni_iocp_recv(iocpdesc_t *iocpd, void *buf, size_t size, bool *would_block, pn_error_t *error);
void pni_iocp_begin_close(iocpdesc_t *iocpd);
iocp_t *pni_iocp();

void pni_events_update(iocpdesc_t *iocpd, int events);
write_result_t *pni_write_result(iocpdesc_t *iocpd, const char *buf, size_t buflen);
write_pipeline_t *pni_write_pipeline(iocpdesc_t *iocpd);
size_t pni_write_pipeline_size(write_pipeline_t *);
bool pni_write_pipeline_writable(write_pipeline_t *);
void pni_write_pipeline_return(write_pipeline_t *, write_result_t *);
size_t pni_write_pipeline_reserve(write_pipeline_t *, size_t);
write_result_t *pni_write_pipeline_next(write_pipeline_t *);
void pni_shared_pool_create(iocp_t *);
void pni_shared_pool_free(iocp_t *);
void pni_zombie_check(iocp_t *, pn_timestamp_t);
pn_timestamp_t pni_zombie_deadline(iocp_t *);

pn_selector_t *pni_selector_create(iocp_t *iocp);

int pni_win32_error(pn_error_t *error, const char *msg, HRESULT code);

#ifdef __cplusplus
}
#endif

#endif /* iocp.h */
