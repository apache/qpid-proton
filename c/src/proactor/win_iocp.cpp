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

#include <proton/condition.h>
#include <proton/connection_driver.h>
#include <proton/engine.h>
#include <proton/message.h>
#include <proton/netaddr.h>
#include <proton/object.h>
#include <proton/proactor.h>
#include <proton/transport.h>
#include <proton/listener.h>
#include <proton/proactor.h>
#include <proton/raw_connection.h>

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <list>
#include <queue>
#include <sstream>

#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>

#include "netaddr-internal.h" /* Include after socket/inet headers */
#include "core/logger_private.h"

/*
 * Proactor for Windows using IO completion ports.
 *
 * There is no specific threading code other than locking and indirect (and
 * brief) calls to the default threadpool via timerqueue timers.
 *
 * The IOCP system remembers what threads have called pn_proactor_wait and
 * assumes they are doing proactor work until they call wait again.  Using a
 * dedicated threadpool to drive the proactor, as in the examples, is
 * recommended.  The use of other long-lived threads to occasionally drive the
 * proactor will hurt the completion port scheduler and cause fewer threads to
 * run.
 *
 * Each proactor connection maintains its own queue of pending completions and
 * its work is serialized on that.  The proactor listener runs parallel where
 * possible, but its event batch is serialized.
 */

// TODO: make all code C++ or all C90-ish
//       change INACTIVE to be from begin_close instead of zombie reap, to be more like Posix
//       make the global write lock window much smaller
//       2 exclusive write buffers per connection
//       make the zombie processing thread safe
//       configurable completion port concurrency
//       proton objects and ref counting: just say no.



// From selector.h
#define PN_READABLE (1)
#define PN_WRITABLE (2)
#define PN_EXPIRED (4)
#define PN_ERROR (8)

typedef SOCKET pn_socket_t;


namespace pn_experimental {
// Code borrowed from old messenger/reactor io.c iocp.c write_pipeline.c select.c
// Mostly unchanged except fro some thread safety (i.e. dropping the old descriptor map)

typedef struct pni_acceptor_t pni_acceptor_t;
typedef struct write_result_t write_result_t;
typedef struct read_result_t read_result_t;
typedef struct write_pipeline_t write_pipeline_t;
typedef struct iocpdesc_t iocpdesc_t;
typedef struct iocp_t iocp_t;


// One per completion port.

struct iocp_t {
  HANDLE completion_port;
  pn_list_t *zombie_list;
  unsigned shared_pool_size;
  char *shared_pool_memory;
  write_result_t **shared_results;
  write_result_t **available_results;
  size_t shared_available_count;
  size_t writer_count;
  int loopback_bufsize;
  bool iocp_trace;
};

// One for each socket.
// It should remain ref counted in the
// zombie_list until ops_in_progress == 0 or the completion port is closed.

struct iocpdesc_t {
  pn_socket_t socket;
  iocp_t *iocp;
  void *active_completer;
  pni_acceptor_t *acceptor;
  pn_error_t *error;
  int ops_in_progress;
  bool read_in_progress;
  write_pipeline_t *pipeline;
  read_result_t *read_result;
  bool bound;          // associated with the completion port
  bool closing;        // close called by application
  bool read_closed;    // EOF or read error
  bool write_closed;   // shutdown sent or write error
  bool poll_error;     // flag posix-like POLLERR/POLLHUP/POLLNVAL
  bool is_mp;          // Special multi thread care required
  bool write_blocked;
  int events;
  pn_timestamp_t reap_time;
};


// Max number of overlapped accepts per listener.  TODO: configurable.
#define IOCP_MAX_ACCEPTS 4

// AcceptEx squishes the local and remote addresses and optional data
// all together when accepting the connection. Reserve enough for
// IPv6 addresses, even if the socket is IPv4. The 16 bytes padding
// per address is required by AcceptEx.
#define IOCP_SOCKADDRMAXLEN (sizeof(sockaddr_in6) + 16)
#define IOCP_SOCKADDRBUFLEN (2 * IOCP_SOCKADDRMAXLEN)

typedef enum { IOCP_ACCEPT, IOCP_CONNECT, IOCP_READ, IOCP_WRITE } iocp_type_t;

typedef struct {
  OVERLAPPED overlapped;
  iocp_type_t type;
  iocpdesc_t *iocpd;
  HRESULT status;
  DWORD num_transferred;
} iocp_result_t;

// Completion keys to distinguish IO from some other user port completions
enum {
    proactor_io = 0,
    proactor_wake_key,
    psocket_wakeup_key,
    recycle_accept_key,
};

struct read_result_t {
  iocp_result_t base;
  size_t drain_count;
  char unused_buf[1];
};

struct write_result_t {
  iocp_result_t base;
  size_t requested;
  bool in_use;
  pn_bytes_t buffer;
};

typedef struct {
  iocp_result_t base;
  char address_buffer[IOCP_SOCKADDRBUFLEN];
  struct addrinfo *addrinfo;
} connect_result_t;

typedef struct {
  iocp_result_t base;
  iocpdesc_t *new_sock;
  char address_buffer[IOCP_SOCKADDRBUFLEN];
  DWORD unused;
} accept_result_t;


void complete_connect(connect_result_t *result, HRESULT status);
void complete_write(write_result_t *result, DWORD xfer_count, HRESULT status);
void complete_read(read_result_t *result, DWORD xfer_count, HRESULT status);
void start_reading(iocpdesc_t *iocpd);
void begin_accept(pni_acceptor_t *acceptor, accept_result_t *result);
void reset_accept_result(accept_result_t *result);
iocpdesc_t *create_same_type_socket(iocpdesc_t *iocpd);
void pni_iocp_reap_check(iocpdesc_t *iocpd);
connect_result_t *connect_result(iocpdesc_t *iocpd, struct addrinfo *addr);
iocpdesc_t *pni_iocpdesc_create(iocp_t *, pn_socket_t s);
iocpdesc_t *pni_deadline_desc(iocp_t *);
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
void pni_zombie_check(iocp_t *, pn_timestamp_t);
pn_timestamp_t pni_zombie_deadline(iocp_t *);

int pni_win32_error(pn_error_t *error, const char *msg, HRESULT code);
}

// ======================================================================
// Write pipeline.
// ======================================================================

/*
 * A simple write buffer pool.  Each socket has a dedicated "primary"
 * buffer and can borrow from a shared pool with limited size tuning.
 * Could enhance e.g. with separate pools per network interface and fancier
 * memory tuning based on interface speed, system resources, and
 * number of connections, etc.
 */

namespace pn_experimental {

// Max overlapped writes per socket
#define IOCP_MAX_OWRITES 16
// Write buffer size
#define IOCP_WBUFSIZE 16384

static void pipeline_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fflush(stderr);
}

void pni_shared_pool_create(iocp_t *iocp)
{
  // TODO: more pools (or larger one) when using multiple non-loopback interfaces
  iocp->shared_pool_size = 16;
  char *env = getenv("PNI_WRITE_BUFFERS"); // Internal: for debugging
  if (env) {
    int sz = atoi(env);
    if (sz >= 0 && sz < 256) {
      iocp->shared_pool_size = sz;
    }
  }
  iocp->loopback_bufsize = 0;
  env = getenv("PNI_LB_BUFSIZE"); // Internal: for debugging
  if (env) {
    int sz = atoi(env);
    if (sz >= 0 && sz <= 128 * 1024) {
      iocp->loopback_bufsize = sz;
    }
  }

  if (iocp->shared_pool_size) {
    iocp->shared_pool_memory = (char *) VirtualAlloc(NULL, IOCP_WBUFSIZE * iocp->shared_pool_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    HRESULT status = GetLastError();
    if (!iocp->shared_pool_memory) {
      perror("Proton write buffer pool allocation failure\n");
      iocp->shared_pool_size = 0;
      iocp->shared_available_count = 0;
      return;
    }

    iocp->shared_results = (write_result_t **) malloc(iocp->shared_pool_size * sizeof(write_result_t *));
    iocp->available_results = (write_result_t **) malloc(iocp->shared_pool_size * sizeof(write_result_t *));
    iocp->shared_available_count = iocp->shared_pool_size;
    char *mem = iocp->shared_pool_memory;
    for (unsigned i = 0; i < iocp->shared_pool_size; i++) {
      iocp->shared_results[i] = iocp->available_results[i] = pni_write_result(NULL, mem, IOCP_WBUFSIZE);
      mem += IOCP_WBUFSIZE;
    }
  }
}

void pni_shared_pool_free(iocp_t *iocp)
{
  for (unsigned i = 0; i < iocp->shared_pool_size; i++) {
    write_result_t *result = iocp->shared_results[i];
    if (result->in_use)
      pipeline_log("Proton buffer pool leak\n");
    else
      free(result);
  }
  if (iocp->shared_pool_size) {
    free(iocp->shared_results);
    free(iocp->available_results);
    if (iocp->shared_pool_memory) {
      if (!VirtualFree(iocp->shared_pool_memory, 0, MEM_RELEASE)) {
        perror("write buffers release failed");
      }
      iocp->shared_pool_memory = NULL;
    }
  }
}

static void shared_pool_push(write_result_t *result)
{
  iocp_t *iocp = result->base.iocpd->iocp;
  assert(iocp->shared_available_count < iocp->shared_pool_size);
  iocp->available_results[iocp->shared_available_count++] = result;
}

static write_result_t *shared_pool_pop(iocp_t *iocp)
{
  return iocp->shared_available_count ? iocp->available_results[--iocp->shared_available_count] : NULL;
}

struct write_pipeline_t {
  iocpdesc_t *iocpd;
  size_t pending_count;
  write_result_t *primary;
  size_t reserved_count;
  size_t next_primary_index;
  size_t depth;
  bool is_writer;
};

#define write_pipeline_compare NULL
#define write_pipeline_inspect NULL
#define write_pipeline_hashcode NULL

static void write_pipeline_initialize(void *object)
{
  write_pipeline_t *pl = (write_pipeline_t *) object;
  pl->pending_count = 0;
  const char *pribuf = (const char *) malloc(IOCP_WBUFSIZE);
  pl->primary = pni_write_result(NULL, pribuf, IOCP_WBUFSIZE);
  pl->depth = 0;
  pl->is_writer = false;
}

static void write_pipeline_finalize(void *object)
{
  write_pipeline_t *pl = (write_pipeline_t *) object;
  free((void *)pl->primary->buffer.start);
  free(pl->primary);
}

write_pipeline_t *pni_write_pipeline(iocpdesc_t *iocpd)
{
  static const pn_cid_t CID_write_pipeline = CID_pn_void;
  static const pn_class_t clazz = PN_CLASS(write_pipeline);
  write_pipeline_t *pipeline = (write_pipeline_t *) pn_class_new(&clazz, sizeof(write_pipeline_t));
  pipeline->iocpd = iocpd;
  pipeline->primary->base.iocpd = iocpd;
  return pipeline;
}

static void confirm_as_writer(write_pipeline_t *pl)
{
  if (!pl->is_writer) {
    iocp_t *iocp = pl->iocpd->iocp;
    iocp->writer_count++;
    pl->is_writer = true;
  }
}

static void remove_as_writer(write_pipeline_t *pl)
{
  if (!pl->is_writer)
    return;
  iocp_t *iocp = pl->iocpd->iocp;
  assert(iocp->writer_count);
  pl->is_writer = false;
  iocp->writer_count--;
}

/*
 * Optimal depth will depend on properties of the NIC, server, and driver.  For now,
 * just distinguish between loopback interfaces and the rest.  Optimizations in the
 * loopback stack allow decent performance with depth 1 and actually cause major
 * performance hiccups if set to large values.
 */
static void set_depth(write_pipeline_t *pl)
{
  pl->depth = 1;
  sockaddr_storage sa;
  socklen_t salen = sizeof(sa);
  char buf[INET6_ADDRSTRLEN];
  DWORD buflen = sizeof(buf);

  if (getsockname(pl->iocpd->socket,(sockaddr*) &sa, &salen) == 0 &&
      getnameinfo((sockaddr*) &sa, salen, buf, buflen, NULL, 0, NI_NUMERICHOST) == 0) {
    if ((sa.ss_family == AF_INET6 && strcmp(buf, "::1")) ||
        (sa.ss_family == AF_INET && strncmp(buf, "127.", 4))) {
      // not loopback
      pl->depth = IOCP_MAX_OWRITES;
    } else {
      iocp_t *iocp = pl->iocpd->iocp;
      if (iocp->loopback_bufsize) {
        const char *p = (const char *) realloc((void *) pl->primary->buffer.start, iocp->loopback_bufsize);
        if (p) {
          pl->primary->buffer.start = p;
          pl->primary->buffer.size = iocp->loopback_bufsize;
        }
      }
    }
  }
}

static size_t pni_min(size_t a, size_t b) { return a < b ? a : b; }

// Reserve as many buffers as possible for count bytes.
size_t pni_write_pipeline_reserve(write_pipeline_t *pl, size_t count)
{
  if (pl->primary->in_use)
    return 0;  // I.e. io->wouldblock
  if (!pl->depth)
    set_depth(pl);
  if (pl->depth == 1) {
    // always use the primary
    pl->reserved_count = 1;
    pl->next_primary_index = 0;
    return 1;
  }

  iocp_t *iocp = pl->iocpd->iocp;
  confirm_as_writer(pl);
  size_t wanted = (count / IOCP_WBUFSIZE);
  if (count % IOCP_WBUFSIZE)
    wanted++;
  size_t pending = pl->pending_count;
  assert(pending < pl->depth);
  size_t bufs = pni_min(wanted, pl->depth - pending);
  // Can draw from shared pool or the primary... but share with others.
  size_t writers = iocp->writer_count;
  size_t shared_count = (iocp->shared_available_count + writers - 1) / writers;
  bufs = pni_min(bufs, shared_count + 1);
  pl->reserved_count = pending + bufs;

  if (bufs == wanted &&
      pl->reserved_count < (pl->depth / 2) &&
      iocp->shared_available_count > (2 * writers + bufs)) {
    // No shortage: keep the primary as spare for future use
    pl->next_primary_index = pl->reserved_count;
  } else if (bufs == 1) {
    pl->next_primary_index = pending;
  } else {
    // let approx 1/3 drain before replenishing
    pl->next_primary_index = ((pl->reserved_count + 2) / 3) - 1;
    if (pl->next_primary_index < pending)
      pl->next_primary_index = pending;
  }
  return bufs;
}

write_result_t *pni_write_pipeline_next(write_pipeline_t *pl)
{
  size_t sz = pl->pending_count;
  if (sz >= pl->reserved_count)
    return NULL;
  write_result_t *result;
  if (sz == pl->next_primary_index) {
    result = pl->primary;
  } else {
    assert(pl->iocpd->iocp->shared_available_count > 0);
    result = shared_pool_pop(pl->iocpd->iocp);
  }

  result->in_use = true;
  pl->pending_count++;
  return result;
}

void pni_write_pipeline_return(write_pipeline_t *pl, write_result_t *result)
{
  result->in_use = false;
  pl->pending_count--;
  pl->reserved_count = 0;
  if (result != pl->primary)
    shared_pool_push(result);
  if (pl->pending_count == 0)
    remove_as_writer(pl);
}

bool pni_write_pipeline_writable(write_pipeline_t *pl)
{
  // Only writable if not full and we can guarantee a buffer:
  return pl->pending_count < pl->depth && !pl->primary->in_use;
}

size_t pni_write_pipeline_size(write_pipeline_t *pl)
{
  return pl->pending_count;
}

}


// ======================================================================
// IOCP infrastructure code
// ======================================================================

/*
 * Socket IO including graceful or forced zombie teardown.
 *
 * Overlapped writes are used to avoid lengthy stalls between write
 * completion and starting a new write.  Non-overlapped reads are used
 * since Windows accumulates inbound traffic without stalling and
 * managing read buffers would not avoid a memory copy at the pn_read
 * boundary.
 *
 * A socket must not get a Windows closesocket() unless the
 * application has called pn_connection_close on the connection or a
 * global single threaded shutdown is in progress. On error, the
 * internal accounting for write_closed or read_closed may be updated
 * along with the external event notification.  A socket may be
 * directly closed if it is never gets started (accept/listener_close
 * collision) or otherwise turned into a zombie.
 */

namespace pn_experimental {

static void iocp_log(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fflush(stderr);
}

static void set_iocp_error_status(pn_error_t *error, int code, HRESULT status)
{
  char buf[512];
  if (FormatMessage(FORMAT_MESSAGE_MAX_WIDTH_MASK | FORMAT_MESSAGE_FROM_SYSTEM,
                    0, status, 0, buf, sizeof(buf), 0))
    pn_error_set(error, code, buf);
  else {
    fprintf(stderr, "pn internal Windows error: %lu\n", GetLastError());
  }
}

static void reap_check(iocpdesc_t *);
static void bind_to_completion_port(iocpdesc_t *iocpd);
static void iocp_shutdown(iocpdesc_t *iocpd);
static bool is_listener(iocpdesc_t *iocpd);
static void release_sys_sendbuf(SOCKET s);

int pni_win32_error(pn_error_t *error, const char *msg, HRESULT code)
{
  // Error code can be from GetLastError or WSAGetLastError,
  char err[1024] = {0};
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
                FORMAT_MESSAGE_MAX_WIDTH_MASK, NULL, code, 0, (LPSTR)&err, sizeof(err), NULL);
  return pn_error_format(error, PN_ERR, "%s: %s", msg, err);
}

static void iocpdesc_fail(iocpdesc_t *iocpd, HRESULT status, const char* text)
{
  pni_win32_error(iocpd->error, text, status);
  if (iocpd->iocp->iocp_trace) {
    iocp_log("connection terminated: %s\n", pn_error_text(iocpd->error));
  }
  iocpd->write_closed = true;
  iocpd->read_closed = true;
  iocpd->poll_error = true;
  pni_events_update(iocpd, iocpd->events & ~(PN_READABLE | PN_WRITABLE));
}

static int pni_strcasecmp(const char *a, const char *b)
{
  int diff;
  while (*b) {
    char aa = *a++, bb = *b++;
    diff = tolower(aa)-tolower(bb);
    if ( diff!=0 ) return diff;
  }
  return *a;
}

static void pni_events_update(iocpdesc_t *iocpd, int events)
{
  // If set, a poll error is permanent
  if (iocpd->poll_error)
    events |= PN_ERROR;
  if (iocpd->events == events)
    return;
  // ditch old code to update list of ready selectables
  iocpd->events = events;
}

// Helper functions to use specialized IOCP AcceptEx() and ConnectEx()
static LPFN_ACCEPTEX lookup_accept_ex(SOCKET s)
{
  GUID guid = WSAID_ACCEPTEX;
  DWORD bytes = 0;
  LPFN_ACCEPTEX fn;
  WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
           &fn, sizeof(fn), &bytes, NULL, NULL);
  assert(fn);
  return fn;
}

static LPFN_CONNECTEX lookup_connect_ex(SOCKET s)
{
  GUID guid = WSAID_CONNECTEX;
  DWORD bytes = 0;
  LPFN_CONNECTEX fn;
  WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
           &fn, sizeof(fn), &bytes, NULL, NULL);
  assert(fn);
  return fn;
}

static LPFN_GETACCEPTEXSOCKADDRS lookup_get_accept_ex_sockaddrs(SOCKET s)
{
  GUID guid = WSAID_GETACCEPTEXSOCKADDRS;
  DWORD bytes = 0;
  LPFN_GETACCEPTEXSOCKADDRS fn;
  WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
           &fn, sizeof(fn), &bytes, NULL, NULL);
  assert(fn);
  return fn;
}

// match accept socket to listener socket
iocpdesc_t *create_same_type_socket(iocpdesc_t *iocpd)
{
  sockaddr_storage sa;
  socklen_t salen = sizeof(sa);
  if (getsockname(iocpd->socket, (sockaddr*)&sa, &salen) == -1)
    return NULL;
  SOCKET s = socket(sa.ss_family, SOCK_STREAM, 0); // Currently only work with SOCK_STREAM
  if (s == INVALID_SOCKET)
    return NULL;
  return pni_iocpdesc_create(iocpd->iocp, s);
}

static bool is_listener(iocpdesc_t *iocpd)
{
  return iocpd && iocpd->acceptor;
}

// === Async accept processing

static accept_result_t *accept_result(iocpdesc_t *listen_sock) {
  accept_result_t *result = (accept_result_t *)calloc(1, sizeof(accept_result_t));
  if (result) {
    result->base.type = IOCP_ACCEPT;
    result->base.iocpd = listen_sock;
  }
  return result;
}

void reset_accept_result(accept_result_t *result) {
  memset(&result->base.overlapped, 0, sizeof (OVERLAPPED));
  memset(&result->address_buffer, 0, IOCP_SOCKADDRBUFLEN);
  result->new_sock = NULL;
}

struct pni_acceptor_t {
  int accept_queue_size;
  pn_list_t *accepts;
  iocpdesc_t *listen_sock;
  bool signalled;
  LPFN_ACCEPTEX fn_accept_ex;
  LPFN_GETACCEPTEXSOCKADDRS fn_get_accept_ex_sockaddrs;
};

#define pni_acceptor_compare NULL
#define pni_acceptor_inspect NULL
#define pni_acceptor_hashcode NULL

static void pni_acceptor_initialize(void *object)
{
  pni_acceptor_t *acceptor = (pni_acceptor_t *) object;
  acceptor->accepts = pn_list(PN_VOID, IOCP_MAX_ACCEPTS);
}

static void pni_acceptor_finalize(void *object)
{
  pni_acceptor_t *acceptor = (pni_acceptor_t *) object;
  size_t len = pn_list_size(acceptor->accepts);
  for (size_t i = 0; i < len; i++)
    free(pn_list_get(acceptor->accepts, i));
  pn_free(acceptor->accepts);
}

static pni_acceptor_t *pni_acceptor(iocpdesc_t *iocpd)
{
  static const pn_cid_t CID_pni_acceptor = CID_pn_void;
  static const pn_class_t clazz = PN_CLASS(pni_acceptor);
  pni_acceptor_t *acceptor = (pni_acceptor_t *) pn_class_new(&clazz, sizeof(pni_acceptor_t));
  acceptor->listen_sock = iocpd;
  acceptor->accept_queue_size = 0;
  acceptor->signalled = false;
  pn_socket_t sock = acceptor->listen_sock->socket;
  acceptor->fn_accept_ex = lookup_accept_ex(sock);
  acceptor->fn_get_accept_ex_sockaddrs = lookup_get_accept_ex_sockaddrs(sock);
  return acceptor;
}

void begin_accept(pni_acceptor_t *acceptor, accept_result_t *result)
{
  // flag to divide this routine's logic into locked/unlocked mp portions
  bool mp = acceptor->listen_sock->is_mp;
  bool created = false;

  if (acceptor->listen_sock->closing) {
    if (result) {
      if (mp && result->new_sock && result->new_sock->socket != INVALID_SOCKET)
        closesocket(result->new_sock->socket);
      free(result);
      acceptor->accept_queue_size--;
    }
    if (acceptor->accept_queue_size == 0)
      acceptor->signalled = true;
    return;
  }

  if (result) {
    if (!mp)
      reset_accept_result(result);
  } else {
    if (acceptor->accept_queue_size < IOCP_MAX_ACCEPTS && (mp ||
           pn_list_size(acceptor->accepts) == acceptor->accept_queue_size )) {
      result = accept_result(acceptor->listen_sock);
      acceptor->accept_queue_size++;
      created = true;
    } else {
      // an async accept is still pending or max concurrent accepts already hit
      return;
    }
  }

  if (created || !mp)
    result->new_sock = create_same_type_socket(acceptor->listen_sock);
  if (result->new_sock) {
    // Not yet connected.
    result->new_sock->read_closed = true;
    result->new_sock->write_closed = true;

    bool success = acceptor->fn_accept_ex(acceptor->listen_sock->socket, result->new_sock->socket,
                     result->address_buffer, 0, IOCP_SOCKADDRMAXLEN, IOCP_SOCKADDRMAXLEN,
                     &result->unused, (LPOVERLAPPED) result);
    if (!success) {
      DWORD err = WSAGetLastError();
      if (err != ERROR_IO_PENDING) {
        if (err == WSAECONNRESET) {
          // other side gave up.  Ignore and try again.
          begin_accept(acceptor, result);
          return;
        }
        else {
          iocpdesc_fail(acceptor->listen_sock, err, "AcceptEX call failure");
          return;
        }
      }
      acceptor->listen_sock->ops_in_progress++;
      // This socket is equally involved in the async operation.
      result->new_sock->ops_in_progress++;
    }
  } else {
    iocpdesc_fail(acceptor->listen_sock, WSAGetLastError(), "create accept socket");
  }
}

static void complete_accept(accept_result_t *result, HRESULT status)
{
  result->new_sock->ops_in_progress--;
  iocpdesc_t *ld = result->base.iocpd;
  if (ld->read_closed) {
    if (!result->new_sock->closing)
      pni_iocp_begin_close(result->new_sock);
    pn_decref(result->new_sock);
    free(result);    // discard
    reap_check(ld);
  } else {
    assert(!ld->is_mp);  // Non mp only
    result->base.status = status;
    pn_list_add(ld->acceptor->accepts, result);
    pni_events_update(ld, ld->events | PN_READABLE);
  }
}


// === Async connect processing


#define connect_result_initialize NULL
#define connect_result_compare NULL
#define connect_result_inspect NULL
#define connect_result_hashcode NULL

static void connect_result_finalize(void *object)
{
  connect_result_t *result = (connect_result_t *) object;
  // Do not release addrinfo until ConnectEx completes
  if (result->addrinfo)
    freeaddrinfo(result->addrinfo);
}

connect_result_t *connect_result(iocpdesc_t *iocpd, struct addrinfo *addr) {
  static const pn_cid_t CID_connect_result = CID_pn_void;
  static const pn_class_t clazz = PN_CLASS(connect_result);
  connect_result_t *result = (connect_result_t *) pn_class_new(&clazz, sizeof(connect_result_t));
  if (result) {
    memset(result, 0, sizeof(connect_result_t));
    result->base.type = IOCP_CONNECT;
    result->base.iocpd = iocpd;
    result->addrinfo = addr;
  }
  return result;
}

pn_socket_t pni_iocp_begin_connect(iocp_t *iocp, pn_socket_t sock, struct addrinfo *addr, pn_error_t *error)
{
  // addr lives for the duration of the async connect.  Caller has passed ownership here.
  // See connect_result_finalize().
  // Use of Windows-specific ConnectEx() requires our socket to be "loosely" pre-bound:
  sockaddr_storage sa;
  memset(&sa, 0, sizeof(sa));
  sa.ss_family = addr->ai_family;
  if (bind(sock, (SOCKADDR *) &sa, addr->ai_addrlen)) {
    pni_win32_error(error, "begin async connection", WSAGetLastError());
    if (iocp->iocp_trace)
      iocp_log("%s\n", pn_error_text(error));
    closesocket(sock);
    freeaddrinfo(addr);
    return INVALID_SOCKET;
  }

  iocpdesc_t *iocpd = pni_iocpdesc_create(iocp, sock);
  bind_to_completion_port(iocpd);
  LPFN_CONNECTEX fn_connect_ex = lookup_connect_ex(iocpd->socket);
  connect_result_t *result = connect_result(iocpd, addr);
  DWORD unused;
  bool success = fn_connect_ex(iocpd->socket, result->addrinfo->ai_addr, result->addrinfo->ai_addrlen,
                               NULL, 0, &unused, (LPOVERLAPPED) result);
  if (!success && WSAGetLastError() != ERROR_IO_PENDING) {
    pni_win32_error(error, "ConnectEx failure", WSAGetLastError());
    pn_free(result);
    iocpd->write_closed = true;
    iocpd->read_closed = true;
    if (iocp->iocp_trace)
      iocp_log("%s\n", pn_error_text(error));
  } else {
    iocpd->ops_in_progress++;
  }
  return sock;
}

void complete_connect(connect_result_t *result, HRESULT status)
{
  iocpdesc_t *iocpd = result->base.iocpd;
  if (iocpd->closing) {
    pn_free(result);
    reap_check(iocpd);
    return;
  }

  if (status) {
    iocpdesc_fail(iocpd, status, "Connect failure");
    // Posix sets selectable events as follows:
    pni_events_update(iocpd, PN_READABLE | PN_EXPIRED);
  } else {
    release_sys_sendbuf(iocpd->socket);
    if (setsockopt(iocpd->socket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT,  NULL, 0)) {
      iocpdesc_fail(iocpd, WSAGetLastError(), "Internal connect failure (update context)");
    } else {
      pni_events_update(iocpd, PN_WRITABLE);
      start_reading(iocpd);
    }
  }
  pn_free(result);
  return;
}


// === Async writes

static bool write_in_progress(iocpdesc_t *iocpd)
{
  return pni_write_pipeline_size(iocpd->pipeline) != 0;
}

write_result_t *pni_write_result(iocpdesc_t *iocpd, const char *buf, size_t buflen)
{
  write_result_t *result = (write_result_t *) calloc(sizeof(write_result_t), 1);
  if (result) {
    result->base.type = IOCP_WRITE;
    result->base.iocpd = iocpd;
    result->buffer.start = buf;
    result->buffer.size = buflen;
  }
  return result;
}

static int submit_write(write_result_t *result, const void *buf, size_t len)
{
  WSABUF wsabuf;
  wsabuf.buf = (char *) buf;
  wsabuf.len = len;
  memset(&result->base.overlapped, 0, sizeof (OVERLAPPED));
  return WSASend(result->base.iocpd->socket, &wsabuf, 1, NULL, 0,
                 (LPOVERLAPPED) result, 0);
}

ssize_t pni_iocp_begin_write(iocpdesc_t *iocpd, const void *buf, size_t len, bool *would_block, pn_error_t *error)
{
  if (len == 0) return 0;
  *would_block = false;
  if (is_listener(iocpd)) {
    set_iocp_error_status(error, PN_ERR, WSAEOPNOTSUPP);
    return INVALID_SOCKET;
  }
  if (iocpd->closing) {
    set_iocp_error_status(error, PN_ERR, WSAESHUTDOWN);
    return SOCKET_ERROR;
  }
  if (iocpd->write_closed) {
    assert(pn_error_code(iocpd->error));
    pn_error_copy(error, iocpd->error);
    if (iocpd->iocp->iocp_trace)
      iocp_log("write error: %s\n", pn_error_text(error));
    return SOCKET_ERROR;
  }
  if (len == 0) return 0;
  if (!(iocpd->events & PN_WRITABLE)) {
    *would_block = true;
    return SOCKET_ERROR;
  }

  size_t written = 0;
  size_t requested = len;
  const char *outgoing = (const char *) buf;
  size_t available = pni_write_pipeline_reserve(iocpd->pipeline, len);
  if (!available) {
    *would_block = true;
    return SOCKET_ERROR;
  }

  for (size_t wr_count = 0; wr_count < available; wr_count++) {
    write_result_t *result = pni_write_pipeline_next(iocpd->pipeline);
    assert(result);
    result->base.iocpd = iocpd;
    ssize_t actual_len = len;
    if (len > result->buffer.size) actual_len = result->buffer.size;
    result->requested = actual_len;
    memmove((void *)result->buffer.start, outgoing, actual_len);
    outgoing += actual_len;
    written += actual_len;
    len -= actual_len;

    int werror = submit_write(result, result->buffer.start, actual_len);
    if (werror && WSAGetLastError() != ERROR_IO_PENDING) {
      pni_write_pipeline_return(iocpd->pipeline, result);
      iocpdesc_fail(iocpd, WSAGetLastError(), "overlapped send");
      return SOCKET_ERROR;
    }
    iocpd->ops_in_progress++;
  }

  if (!pni_write_pipeline_writable(iocpd->pipeline))
    pni_events_update(iocpd, iocpd->events & ~PN_WRITABLE);
  return written;
}

/*
 * Note: iocp write completion is not "bytes on the wire", it is "peer
 * acked the sent bytes".  Completion can be seconds on a slow
 * consuming peer.
 */
void complete_write(write_result_t *result, DWORD xfer_count, HRESULT status)
{
  iocpdesc_t *iocpd = result->base.iocpd;
  if (iocpd->closing) {
    pni_write_pipeline_return(iocpd->pipeline, result);
    if (!iocpd->write_closed && !write_in_progress(iocpd))
      iocp_shutdown(iocpd);
    reap_check(iocpd);
    return;
  }
  if (status == 0 && xfer_count > 0) {
    if (xfer_count != result->requested) {
      // Is this recoverable?  How to preserve order if multiple overlapped writes?
      pni_write_pipeline_return(iocpd->pipeline, result);
      iocpdesc_fail(iocpd, WSA_OPERATION_ABORTED, "Partial overlapped write on socket");
      return;
    } else {
      // Success.
      pni_write_pipeline_return(iocpd->pipeline, result);
      if (pni_write_pipeline_writable(iocpd->pipeline))
        pni_events_update(iocpd, iocpd->events | PN_WRITABLE);
      return;
    }
  }
  // Other error
  pni_write_pipeline_return(iocpd->pipeline, result);
  if (status == WSAECONNABORTED || status == WSAECONNRESET || status == WSAENOTCONN
      || status == ERROR_NETNAME_DELETED) {
    iocpd->write_closed = true;
    iocpd->poll_error = true;
    pni_events_update(iocpd, iocpd->events & ~PN_WRITABLE);
    pni_win32_error(iocpd->error, "Remote close or timeout", status);
  } else {
    iocpdesc_fail(iocpd, status, "IOCP async write error");
  }
}


// === Async reads


static read_result_t *read_result(iocpdesc_t *iocpd)
{
  read_result_t *result = (read_result_t *) calloc(sizeof(read_result_t), 1);
  if (result) {
    result->base.type = IOCP_READ;
    result->base.iocpd = iocpd;
  }
  return result;
}

static void begin_zero_byte_read(iocpdesc_t *iocpd)
{
  if (iocpd->read_in_progress) return;
  if (iocpd->read_closed) {
    pni_events_update(iocpd, iocpd->events | PN_READABLE);
    return;
  }

  read_result_t *result = iocpd->read_result;
  memset(&result->base.overlapped, 0, sizeof (OVERLAPPED));
  DWORD flags = 0;
  WSABUF wsabuf;
  wsabuf.buf = result->unused_buf;
  wsabuf.len = 0;
  int rc = WSARecv(iocpd->socket, &wsabuf, 1, NULL, &flags,
                       &result->base.overlapped, 0);
  if (rc && WSAGetLastError() != ERROR_IO_PENDING) {
    iocpdesc_fail(iocpd, WSAGetLastError(), "IOCP read error");
    return;
  }
  iocpd->ops_in_progress++;
  iocpd->read_in_progress = true;
}

static void drain_until_closed(iocpdesc_t *iocpd) {
  size_t max_drain = 16 * 1024;
  char buf[512];
  read_result_t *result = iocpd->read_result;
  while (result->drain_count < max_drain) {
    int rv = recv(iocpd->socket, buf, 512, 0);
    if (rv > 0)
      result->drain_count += rv;
    else if (rv == 0) {
      iocpd->read_closed = true;
      return;
    } else if (WSAGetLastError() == WSAEWOULDBLOCK) {
      // wait a little longer
      start_reading(iocpd);
      return;
    }
    else
      break;
  }
  // Graceful close indication unlikely, force the issue
  if (iocpd->iocp->iocp_trace)
    if (result->drain_count >= max_drain)
      iocp_log("graceful close on reader abandoned (too many chars)\n");
    else
      iocp_log("graceful close on reader abandoned: %d\n", WSAGetLastError());
  iocpd->read_closed = true;
}


void complete_read(read_result_t *result, DWORD xfer_count, HRESULT status)
{
  iocpdesc_t *iocpd = result->base.iocpd;
  iocpd->read_in_progress = false;

  if (iocpd->closing) {
    // Application no longer reading, but we are looking for a zero length read
    if (!iocpd->read_closed)
      drain_until_closed(iocpd);
    reap_check(iocpd);
    return;
  }

  if (status == 0 && xfer_count == 0) {
    // Success.
    pni_events_update(iocpd, iocpd->events | PN_READABLE);
  } else {
    iocpdesc_fail(iocpd, status, "IOCP read complete error");
  }
}

ssize_t pni_iocp_recv(iocpdesc_t *iocpd, void *buf, size_t size, bool *would_block, pn_error_t *error)
{
  if (size == 0) return 0;
  *would_block = false;
  if (is_listener(iocpd)) {
    set_iocp_error_status(error, PN_ERR, WSAEOPNOTSUPP);
    return SOCKET_ERROR;
  }
  if (iocpd->closing) {
    // Previous call to pn_close()
    set_iocp_error_status(error, PN_ERR, WSAESHUTDOWN);
    return SOCKET_ERROR;
  }
  if (iocpd->read_closed) {
    if (pn_error_code(iocpd->error))
      pn_error_copy(error, iocpd->error);
    else
      set_iocp_error_status(error, PN_ERR, WSAENOTCONN);
    return SOCKET_ERROR;
  }

  int count = recv(iocpd->socket, (char *) buf, size, 0);
  if (count > 0) {
    pni_events_update(iocpd, iocpd->events & ~PN_READABLE);
    // caller must initiate     begin_zero_byte_read(iocpd);
    return (ssize_t) count;
  } else if (count == 0) {
    iocpd->read_closed = true;
    return 0;
  }
  if (WSAGetLastError() == WSAEWOULDBLOCK)
    *would_block = true;
  else {
    set_iocp_error_status(error, PN_ERR, WSAGetLastError());
    iocpd->read_closed = true;
  }
  return SOCKET_ERROR;
}

void start_reading(iocpdesc_t *iocpd)
{
  begin_zero_byte_read(iocpd);
}


// === The iocp descriptor

static void pni_iocpdesc_initialize(void *object)
{
  iocpdesc_t *iocpd = (iocpdesc_t *) object;
  memset(iocpd, 0, sizeof(iocpdesc_t));
  iocpd->socket = INVALID_SOCKET;
}

static void pni_iocpdesc_finalize(void *object)
{
  iocpdesc_t *iocpd = (iocpdesc_t *) object;
  pn_free(iocpd->acceptor);
  pn_error_free(iocpd->error);
   if (iocpd->pipeline)
    if (write_in_progress(iocpd))
      iocp_log("iocp descriptor write leak\n");
    else
      pn_free(iocpd->pipeline);
  if (iocpd->read_in_progress)
    iocp_log("iocp descriptor read leak\n");
  else
    free(iocpd->read_result);
}

static uintptr_t pni_iocpdesc_hashcode(void *object)
{
  iocpdesc_t *iocpd = (iocpdesc_t *) object;
  return iocpd->socket;
}

#define pni_iocpdesc_compare NULL
#define pni_iocpdesc_inspect NULL

// Reference counted in the iocpdesc map, zombie_list, selector.
static iocpdesc_t *pni_iocpdesc(pn_socket_t s)
{
  static const pn_cid_t CID_pni_iocpdesc = CID_pn_void;
  static pn_class_t clazz = PN_CLASS(pni_iocpdesc);
  iocpdesc_t *iocpd = (iocpdesc_t *) pn_class_new(&clazz, sizeof(iocpdesc_t));
  assert(iocpd);
  iocpd->socket = s;
  return iocpd;
}

static bool is_listener_socket(pn_socket_t s)
{
  BOOL tval = false;
  int tvalsz = sizeof(tval);
  int code = getsockopt(s, SOL_SOCKET, SO_ACCEPTCONN, (char *)&tval, &tvalsz);
  return code == 0 && tval;
}

iocpdesc_t *pni_iocpdesc_create(iocp_t *iocp, pn_socket_t s) {
  assert (s != INVALID_SOCKET);
  bool listening = is_listener_socket(s);
  iocpdesc_t *iocpd = pni_iocpdesc(s);
  iocpd->iocp = iocp;
  if (iocpd) {
    iocpd->error = pn_error();
    if (listening) {
      iocpd->acceptor = pni_acceptor(iocpd);
    } else {
      iocpd->pipeline = pni_write_pipeline(iocpd);
      iocpd->read_result = read_result(iocpd);
    }
  }
  return iocpd;
}


static void bind_to_completion_port(iocpdesc_t *iocpd)
{
  if (iocpd->bound) return;
  if (!iocpd->iocp->completion_port) {
    iocpdesc_fail(iocpd, WSAEINVAL, "Incomplete setup, no completion port.");
    return;
  }

  if (CreateIoCompletionPort ((HANDLE) iocpd->socket, iocpd->iocp->completion_port, proactor_io, 0))
    iocpd->bound = true;
  else {
    iocpdesc_fail(iocpd, GetLastError(), "IOCP socket setup.");
  }
}

static void release_sys_sendbuf(SOCKET s)
{
  // Set the socket's send buffer size to zero.
  int sz = 0;
  int status = setsockopt(s, SOL_SOCKET, SO_SNDBUF, (const char *)&sz, sizeof(int));
  assert(status == 0);
}

void pni_iocpdesc_start(iocpdesc_t *iocpd)
{
  if (iocpd->bound) return;
  bind_to_completion_port(iocpd);
  if (is_listener(iocpd)) {
    begin_accept(iocpd->acceptor, NULL);
  }
  else {
    release_sys_sendbuf(iocpd->socket);
    pni_events_update(iocpd, PN_WRITABLE);
    start_reading(iocpd);
  }
}

static void complete(iocp_result_t *result, bool success, DWORD num_transferred) {
  result->iocpd->ops_in_progress--;
  DWORD status = success ? 0 : GetLastError();

  switch (result->type) {
  case IOCP_ACCEPT:
    complete_accept((accept_result_t *) result, status);
    break;
  case IOCP_CONNECT:
    complete_connect((connect_result_t *) result, status);
    break;
  case IOCP_WRITE:
    complete_write((write_result_t *) result, num_transferred, status);
    break;
  case IOCP_READ:
    complete_read((read_result_t *) result, num_transferred, status);
    break;
  default:
    assert(false);
  }
}

void pni_iocp_drain_completions(iocp_t *iocp)
{
  while (true) {
    DWORD timeout_ms = 0;
    DWORD num_transferred = 0;
    ULONG_PTR completion_key = 0;
    OVERLAPPED *overlapped = 0;

    bool good_op = GetQueuedCompletionStatus (iocp->completion_port, &num_transferred,
                                               &completion_key, &overlapped, timeout_ms);
    if (!overlapped)
      return;  // timed out
    iocp_result_t *result = (iocp_result_t *) overlapped;
    if (!completion_key)
      complete(result, good_op, num_transferred);
  }
}

// returns: -1 on error, 0 on timeout, 1 successful completion
// proactor layer uses completion_key, but we don't, so ignore those (shutdown in progress)
int pni_iocp_wait_one(iocp_t *iocp, int timeout, pn_error_t *error) {
  DWORD win_timeout = (timeout < 0) ? INFINITE : (DWORD) timeout;
  DWORD num_transferred = 0;
  ULONG_PTR completion_key = 0;
  OVERLAPPED *overlapped = 0;

  bool good_op = GetQueuedCompletionStatus (iocp->completion_port, &num_transferred,
                                            &completion_key, &overlapped, win_timeout);
  if (!overlapped)
    if (GetLastError() == WAIT_TIMEOUT)
      return 0;
    else {
      if (error)
        pni_win32_error(error, "GetQueuedCompletionStatus", GetLastError());
      return -1;
    }

  if (completion_key)
    return pni_iocp_wait_one(iocp, timeout, error);
  iocp_result_t *result = (iocp_result_t *) overlapped;
  complete(result, good_op, num_transferred);
  return 1;
}

// === Close (graceful and otherwise)

// zombie_list is for sockets transitioning out of iocp on their way to zero ops_in_progress
// and fully closed.

static void zombie_list_add(iocpdesc_t *iocpd)
{
  assert(iocpd->closing);
  if (!iocpd->ops_in_progress) {
    // No need to make a zombie.
    if (iocpd->socket != INVALID_SOCKET) {
      closesocket(iocpd->socket);
      iocpd->socket = INVALID_SOCKET;
      iocpd->read_closed = true;
    }
    return;
  }
  // Allow 2 seconds for graceful shutdown before releasing socket resource.
  iocpd->reap_time = pn_proactor_now_64() + 2000;
  pn_list_add(iocpd->iocp->zombie_list, iocpd);
}

static void reap_check(iocpdesc_t *iocpd)
{
  if (iocpd->closing && !iocpd->ops_in_progress) {
    if (iocpd->socket != INVALID_SOCKET) {
      closesocket(iocpd->socket);
      iocpd->socket = INVALID_SOCKET;
    }
    pn_list_remove(iocpd->iocp->zombie_list, iocpd);
    // iocpd is decref'ed and possibly released
  }
}

void pni_iocp_reap_check(iocpdesc_t *iocpd) {
  reap_check(iocpd);
}

pn_timestamp_t pni_zombie_deadline(iocp_t *iocp)
{
  if (pn_list_size(iocp->zombie_list)) {
    iocpdesc_t *iocpd = (iocpdesc_t *) pn_list_get(iocp->zombie_list, 0);
    return iocpd->reap_time;
  }
  return 0;
}

void pni_zombie_check(iocp_t *iocp, pn_timestamp_t now)
{
  pn_list_t *zl = iocp->zombie_list;
  // Look for stale zombies that should have been reaped by "now"
  for (size_t idx = 0; idx < pn_list_size(zl); idx++) {
    iocpdesc_t *iocpd = (iocpdesc_t *) pn_list_get(zl, idx);
    if (iocpd->reap_time > now)
      return;
    if (iocpd->socket == INVALID_SOCKET)
      continue;
    if (iocp->iocp_trace)
      iocp_log("async close: graceful close timeout exceeded\n");
    closesocket(iocpd->socket);
    iocpd->socket = INVALID_SOCKET;
    iocpd->read_closed = true;
    // outstanding ops should complete immediately now
  }
}

static void drain_zombie_completions(iocp_t *iocp)
{
  // No more pn_selector_select() from App, but zombies still need care and feeding
  // until their outstanding async actions complete.
  pni_iocp_drain_completions(iocp);

  // Discard any that have no pending async IO
  size_t sz = pn_list_size(iocp->zombie_list);
  for (size_t idx = 0; idx < sz;) {
    iocpdesc_t *iocpd = (iocpdesc_t *) pn_list_get(iocp->zombie_list, idx);
    if (!iocpd->ops_in_progress) {
      pn_list_del(iocp->zombie_list, idx, 1);
      sz--;
    } else {
      idx++;
    }
  }

  unsigned shutdown_grace = 2000;
  char *override = getenv("PN_SHUTDOWN_GRACE");
  if (override) {
    int grace = atoi(override);
    if (grace > 0 && grace < 60000)
      shutdown_grace = (unsigned) grace;
  }
  pn_timestamp_t now = pn_proactor_now_64();
  pn_timestamp_t deadline = now + shutdown_grace;

  while (pn_list_size(iocp->zombie_list)) {
    if (now >= deadline)
      break;
    int rv = pni_iocp_wait_one(iocp, deadline - now, NULL);
    if (rv < 0) {
      iocp_log("unexpected IOCP failure on Proton IO shutdown %d\n", GetLastError());
      break;
    }
    now = pn_proactor_now_64();
  }
  if (now >= deadline && pn_list_size(iocp->zombie_list) && iocp->iocp_trace)
    // Should only happen if really slow TCP handshakes, i.e. total network failure
    iocp_log("network failure on Proton shutdown\n");
}

static void zombie_list_hard_close_all(iocp_t *iocp)
{
  pni_iocp_drain_completions(iocp);
  size_t zs = pn_list_size(iocp->zombie_list);
  for (size_t i = 0; i < zs; i++) {
    iocpdesc_t *iocpd = (iocpdesc_t *) pn_list_get(iocp->zombie_list, i);
    if (iocpd->socket != INVALID_SOCKET) {
      closesocket(iocpd->socket);
      iocpd->socket = INVALID_SOCKET;
      iocpd->read_closed = true;
      iocpd->write_closed = true;
    }
  }
  pni_iocp_drain_completions(iocp);

  // Zombies should be all gone.  Do a sanity check.
  zs = pn_list_size(iocp->zombie_list);
  int remaining = 0;
  int ops = 0;
  for (size_t i = 0; i < zs; i++) {
    iocpdesc_t *iocpd = (iocpdesc_t *) pn_list_get(iocp->zombie_list, i);
    remaining++;
    ops += iocpd->ops_in_progress;
  }
  if (remaining)
    iocp_log("Proton: %d unfinished close operations (ops count = %d)\n", remaining, ops);
}

static void iocp_shutdown(iocpdesc_t *iocpd)
{
  if (iocpd->socket == INVALID_SOCKET)
    return;    // Hard close in progress
  if (shutdown(iocpd->socket, SD_SEND)) {
    int err = WSAGetLastError();
    if (err != WSAECONNABORTED && err != WSAECONNRESET && err != WSAENOTCONN)
      if (iocpd->iocp->iocp_trace)
        iocp_log("socket shutdown failed %d\n", err);
  }
  iocpd->write_closed = true;
}

void pni_iocp_begin_close(iocpdesc_t *iocpd)
{
  assert (!iocpd->closing);
  if (is_listener(iocpd)) {
    // Listening socket is easy.  Close the socket which will cancel async ops.
    pn_socket_t old_sock = iocpd->socket;
    iocpd->socket = INVALID_SOCKET;
    iocpd->closing = true;
    iocpd->read_closed = true;
    iocpd->write_closed = true;
    closesocket(old_sock);
    // Pending accepts will now complete.  Zombie can die when all consumed.
    zombie_list_add(iocpd);
  } else {
    // Continue async operation looking for graceful close confirmation or timeout.
    pn_socket_t old_sock = iocpd->socket;
    iocpd->closing = true;
    if (!iocpd->write_closed && !write_in_progress(iocpd))
      iocp_shutdown(iocpd);
    zombie_list_add(iocpd);
  }
}


// === iocp_t

#define pni_iocp_hashcode NULL
#define pni_iocp_compare NULL
#define pni_iocp_inspect NULL

void pni_iocp_initialize(void *obj)
{
  iocp_t *iocp = (iocp_t *) obj;
  memset(iocp, 0, sizeof(iocp_t));
  pni_shared_pool_create(iocp);
  iocp->completion_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
  assert(iocp->completion_port != NULL);
  iocp->zombie_list = pn_list(PN_OBJECT, 0);
  iocp->iocp_trace = false;
}

void pni_iocp_finalize(void *obj)
{
  iocp_t *iocp = (iocp_t *) obj;
  // Move sockets to closed state
  drain_zombie_completions(iocp);    // Last chance for graceful close
  zombie_list_hard_close_all(iocp);
  CloseHandle(iocp->completion_port);  // This cancels all our async ops
  iocp->completion_port = NULL;
  // Now safe to free everything that might be touched by a former async operation.
  pn_free(iocp->zombie_list);
  pni_shared_pool_free(iocp);
}

iocp_t *pni_iocp()
{
  static const pn_cid_t CID_pni_iocp = CID_pn_void;
  static const pn_class_t clazz = PN_CLASS(pni_iocp);
  iocp_t *iocp = (iocp_t *) pn_class_new(&clazz, sizeof(iocp_t));
  return iocp;
}

}


// ======================================================================
// Proton Proactor support
// ======================================================================

#include "core/logger_private.h"
#include "proactor-internal.h"

class csguard {
  public:
    csguard(CRITICAL_SECTION *cs) : cs_(cs), set_(true) { EnterCriticalSection(cs_); }
    ~csguard() { if (set_) LeaveCriticalSection(cs_); }
    void release() {
        if (set_) {
            set_ = false;
            LeaveCriticalSection(cs_);
        }
    }
  private:
    LPCRITICAL_SECTION cs_;
    bool set_;
};


// Get string from error status
std::string errno_str2(DWORD status) {
  char buf[512];
  if (FormatMessage(FORMAT_MESSAGE_MAX_WIDTH_MASK | FORMAT_MESSAGE_FROM_SYSTEM,
                    0, status, 0, buf, sizeof(buf), 0))
      return std::string(buf);
  return std::string("internal proactor error");
}


std::string errno_str(const std::string& msg, bool is_wsa) {
  DWORD e = is_wsa ? WSAGetLastError() : GetLastError();
  return msg + ": " + errno_str2(e);
}



using namespace pn_experimental;

static int pgetaddrinfo(const char *host, const char *port, int flags, struct addrinfo **res)
{
  struct addrinfo hints = { 0 };
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG | flags;
  return getaddrinfo(host, port, &hints, res) ? WSAGetLastError() : 0;
}

const char *COND_NAME = "proactor";
PN_HANDLE(PN_PROACTOR)

// The number of times a connection event batch may be replenished for
// a thread between calls to wait().
// TODO: consider some instrumentation to determine an optimal number
// or switch to cpu time based limit.
#define HOG_MAX 3

/* pn_proactor_t and pn_listener_t are plain C structs with normal memory management.
   Class definitions are for identification as pn_event_t context only.
*/
PN_STRUCT_CLASSDEF(pn_proactor)
PN_STRUCT_CLASSDEF(pn_listener)

/* Completion serialization context common to connection and listener. */
/* And also the reaper singleton (which has no socket */

typedef enum {
  PROACTOR,
  PCONNECTION,
  LISTENER,
  WAKEABLE } pcontext_type_t;

typedef struct pcontext_t {
  CRITICAL_SECTION cslock;
  pn_proactor_t *proactor;  /* Immutable */
  void *owner;              /* Instance governed by the context */
  pcontext_type_t type;
  bool working;
  bool wake_pending;
  int completion_ops;  // uncompleted ops that are not socket IO related
  struct pcontext_t *wake_next; // wake list, guarded by proactor eventfd_mutex
  bool closing;
  // Next 4 are protected by the proactor mutex
  struct pcontext_t* next;  /* Protected by proactor.mutex */
  struct pcontext_t* prev;  /* Protected by proactor.mutex */
  int disconnect_ops;           /* ops remaining before disconnect complete */
  bool disconnecting;           /* pn_proactor_disconnect */
} pcontext_t;

static void pcontext_init(pcontext_t *ctx, pcontext_type_t t, pn_proactor_t *p, void *o) {
  memset(ctx, 0, sizeof(*ctx));
  InitializeCriticalSectionAndSpinCount(&ctx->cslock, 4000);
  ctx->proactor = p;
  ctx->owner = o;
  ctx->type = t;
}

static void pcontext_finalize(pcontext_t* ctx) {
  DeleteCriticalSection(&ctx->cslock);
}

typedef struct psocket_t {
  iocpdesc_t *iocpd;            /* NULL if reaper, or socket open failure. */
  pn_listener_t *listener;      /* NULL for a connection socket */
  pn_netaddr_t listen_addr;     /* Not filled in for connection sockets */
  char addr_buf[PN_MAX_ADDR];
  const char *host, *port;
  bool is_reaper;
} psocket_t;

static void psocket_init(psocket_t* ps, pn_listener_t *listener, bool is_reaper, const char *addr) {
  ps->is_reaper = is_reaper;
  if (is_reaper) return;
  ps->listener = listener;
  pni_parse_addr(addr, ps->addr_buf, sizeof(ps->addr_buf), &ps->host, &ps->port);
}

struct pn_proactor_t {
  pcontext_t context;
  CRITICAL_SECTION write_lock;
  CRITICAL_SECTION timer_lock;
  CRITICAL_SECTION bind_lock;
  HANDLE timer_queue;
  HANDLE timeout_timer;
  iocp_t *iocp;
  class reaper *reaper;
  pn_collector_t *collector;
  pcontext_t *contexts;         /* in-use contexts for PN_PROACTOR_INACTIVE and cleanup */
  pn_event_batch_t batch;
  size_t disconnects_pending;   /* unfinished proactor disconnects*/

  // need_xxx flags indicate we should generate PN_PROACTOR_XXX on the next update_batch()
  bool need_interrupt;
  bool need_inactive;
  bool need_timeout;
  bool timeout_set; /* timeout has been set by user and not yet cancelled or generated event */
  bool timeout_processed;  /* timout event dispatched in the most recent event batch */
  bool delayed_interrupt;
  bool shutting_down;
};

typedef struct pconnection_t {
  psocket_t psocket;
  pcontext_t context;
  pn_connection_driver_t driver;
  std::queue<iocp_result_t *> *completion_queue;
  std::queue<iocp_result_t *> *work_queue;
  pn_condition_t *disconnect_condition;
  pn_event_batch_t batch;
  int wake_count;
  int hog_count; // thread hogging limiter
  bool server;                /* accept, not connect */
  bool started;
  bool connecting;
  bool tick_pending;
  bool queued_disconnect;     /* deferred from pn_proactor_disconnect() */
  bool bound;
  bool stop_timer_required;
  bool can_wake;
  HANDLE tick_timer;
  struct pn_netaddr_t local, remote; /* Actual addresses */
  struct addrinfo *addrinfo;         /* Resolved address list */
  struct addrinfo *ai;               /* Current connect address */
} pconnection_t;

struct pn_listener_t {
  psocket_t *psockets;          /* Array of listening sockets */
  size_t psockets_size;
  pcontext_t context;
  std::queue<accept_result_t *> *pending_accepts;  // sockets awaiting a pn_listener_accept
  int pending_events;          // number of PN_LISTENER_ACCEPT events to be delivered
  pn_condition_t *condition;
  pn_collector_t *collector;
  pn_event_batch_t batch;
  pn_record_t *attachments;
  void *listener_context;
  size_t backlog;
  bool close_dispatched;
};


static bool proactor_remove(pcontext_t *ctx);
static void listener_done(pn_listener_t *l);
static bool proactor_update_batch(pn_proactor_t *p);
static pn_event_batch_t *listener_process(pn_listener_t *l, iocp_result_t *result);
static void recycle_result(accept_result_t *accept_result);
static void proactor_add(pcontext_t *p);
static void release_pending_accepts(pn_listener_t *l);
static void proactor_shutdown(pn_proactor_t *p);

static void post_completion(iocp_t *iocp, ULONG_PTR arg1, void *arg2) {
    // Despite the vagueness of the official documentation, the
    // following args to Post are passed undisturbed and unvalidated
    // to GetQueuedCompletionStatus().  In particular, arg2 does not
    // have to be an OVERLAPPED struct and may be NULL.

    DWORD nxfer = 0;            // We could pass this through too if needed.
    PostQueuedCompletionStatus(iocp->completion_port, nxfer, arg1, (LPOVERLAPPED) arg2);
}

static inline bool is_write_result(iocp_result_t *r) {
    return r && r->type == IOCP_WRITE;
}

static inline bool is_read_result(iocp_result_t *r) {
    return r && r->type == IOCP_READ;
}

static inline bool is_connect_result(iocp_result_t *r) {
    return r && r->type == IOCP_CONNECT;
}

// From old io.c
static void pni_configure_sock_2(pn_socket_t sock) {
  //
  // Disable the Nagle algorithm on TCP connections.
  //

  int flag = 1;
  if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag)) != 0) {
      //TODO: log/err
  }

  u_long nonblock = 1;
  if (ioctlsocket(sock, FIONBIO, &nonblock)) {
      // TODO: log/err
  }
}

static LPFN_CONNECTEX lookup_connect_ex2(SOCKET s)
{
  GUID guid = WSAID_CONNECTEX;
  DWORD bytes = 0;
  LPFN_CONNECTEX fn;
  WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
           &fn, sizeof(fn), &bytes, NULL, NULL);
  assert(fn);
  return fn;
}


// File descriptor wrapper that calls ::close in destructor.
class unique_socket {
  public:
    unique_socket(pn_socket_t socket) : socket_(socket) {}
    ~unique_socket() { if (socket_ != INVALID_SOCKET) ::closesocket(socket_); }
    operator pn_socket_t() const { return socket_; }
    pn_socket_t release() { pn_socket_t ret = socket_; socket_ = INVALID_SOCKET; return ret; }

  protected:
    pn_socket_t socket_;
};

void do_complete(iocp_result_t *result) {
  iocpdesc_t *iocpd = result->iocpd;  // connect result gets deleted
  switch (result->type) {

  case IOCP_ACCEPT:
    /* accept is now processed inline to do in parallel, except on teardown */
    assert(iocpd->closing);
    complete_accept((accept_result_t *) result, result->status);  // free's result and retires new_sock
    break;
  case IOCP_CONNECT:
    complete_connect((connect_result_t *) result, result->status);
    break;
  case IOCP_WRITE:
    complete_write((write_result_t *) result, result->num_transferred, result->status);
    break;
  case IOCP_READ:
    complete_read((read_result_t *) result, result->num_transferred, result->status);
    break;
  default:
    assert(false);
  }

  iocpd->ops_in_progress--;  // Set in each begin_xxx call
}


static inline pconnection_t *as_pconnection_t(psocket_t* ps) {
  return !(ps->is_reaper || ps->listener) ? (pconnection_t*)ps : NULL;
}

static inline pn_listener_t *as_listener(psocket_t* ps) {
  return ps->listener;
}

static inline pconnection_t *pcontext_pconnection(pcontext_t *c) {
  return c->type == PCONNECTION ?
    containerof(c, pconnection_t, context) : NULL;
}
static inline pn_listener_t *pcontext_listener(pcontext_t *c) {
  return c->type == LISTENER ?
    containerof(c, pn_listener_t, context) : NULL;
}

static pcontext_t *psocket_context(psocket_t *ps) {
  if (ps->listener)
    return &ps->listener->context;
  pconnection_t *pc = as_pconnection_t(ps);
  return &pc->context;
}


// Call wih lock held
static inline void wake_complete(pcontext_t *ctx) {
  ctx->wake_pending = false;
  ctx->completion_ops--;
}

// Call wih lock held
static void wakeup(psocket_t *ps) {
  pcontext_t *ctx = psocket_context(ps);
  if (!ctx->working && !ctx->wake_pending) {
    ctx->wake_pending = true;
    ctx->completion_ops++;
    post_completion(ctx->proactor->iocp, psocket_wakeup_key, ps);
  }
}

// Call wih lock held
static inline void proactor_wake_complete(pn_proactor_t *p) {
  wake_complete(&p->context);
}

// Call wih lock held
static void proactor_wake(pn_proactor_t *p) {
  if (!p->context.working && !p->context.wake_pending) {
    p->context.wake_pending = true;
    p->context.completion_ops++;
    post_completion(p->iocp, proactor_wake_key, p);
  }
}

VOID CALLBACK reap_check_cb(PVOID arg, BOOLEAN /* ignored*/ );

// Serialize handling listener and connection closing IO completions
// after the engine has lost interest.  A temporary convenience while
// using the old single threaded iocp/io/select driver code.
class reaper {
  public:
    reaper(pn_proactor_t *p, CRITICAL_SECTION *wlock, iocp_t *iocp)
      : iocp_(iocp), global_wlock_(wlock), timer_(NULL), running(true) {
      InitializeCriticalSectionAndSpinCount(&lock_, 4000);
      timer_queue_ = CreateTimerQueue();
      if (!timer_queue_) {
          perror("CreateTimerQueue");
          abort();
      }
    }

  // Connection or listener lock must also be held by caller.
    bool add(iocpdesc_t *iocpd) {
        if (!iocpd) return false;
        csguard g(&lock_);
        if (iocpd->closing) return false;
        bool rval = !iocpd->ops_in_progress;
        pni_iocp_begin_close(iocpd); // sets iocpd->closing
        pn_decref(iocpd);            // may still be ref counted on zombie list
        reap_timer();
        return rval;
    }

    // For cases where the close will be immediate.  I.E. after a failed
    // connection attempt where there is no follow-on IO.
    void fast_reap(iocpdesc_t *iocpd) {
        assert(iocpd && iocpd->ops_in_progress == 0 && !iocpd->closing);
        csguard g(&lock_);
        pni_iocp_begin_close(iocpd);
        pn_decref(iocpd);
    }

    bool process(iocp_result_t *result) {
        // No queue of completions for the reaper.  Just process
        // serialized by the lock assuming all actions are "short".
        // That may be wrong, and if so the real fix is not a
        // consumer/producer setup but just replace the reaper with a
        // multi threaded alternative.
        csguard g(&lock_);
        iocpdesc_t *iocpd = result->iocpd;
        if (is_write_result(result)) {
            csguard wg(global_wlock_);
            do_complete(result);
        }
        else do_complete(result);
        // result may now be NULL
        bool rval = (iocpd->ops_in_progress == 0);
        pni_iocp_reap_check(iocpd);
        return rval;
    }

    // Called when all competing threads have terminated except our own reap_check timer.
    void final_shutdown() {
        running = false;
        DeleteTimerQueueEx(timer_queue_, INVALID_HANDLE_VALUE);
        // No pending or active timers from thread pool remain.  Truly single threaded now.
        pn_free((void *) iocp_); // calls pni_iocp_finalize(); cleans up all sockets, completions, completion port.
        DeleteCriticalSection(&lock_);
    }

    void reap_check() {
        csguard g(&lock_);
        DeleteTimerQueueTimer(timer_queue_, timer_, NULL);
        timer_ = NULL;
        reap_timer();
    }


  private:
    void reap_timer() {
        // Call with lock
        if (timer_ || !running)
            return;
        int64_t now = pn_proactor_now_64();
        pni_zombie_check(iocp_, now);
        int64_t zd = pni_zombie_deadline(iocp_);
        if (zd) {
            DWORD tm = (zd > now) ? zd - now : 1;
            if (!CreateTimerQueueTimer(&timer_, timer_queue_, reap_check_cb, this, tm,
                                       0, WT_EXECUTEONLYONCE)) {
                perror("CreateTimerQueueTimer");
                abort();
            }
        }
    }

    iocp_t *iocp_;
    CRITICAL_SECTION lock_;
    CRITICAL_SECTION *global_wlock_;
    HANDLE timer_queue_;
    HANDLE timer_;
    bool running;
};

VOID CALLBACK reap_check_cb(PVOID arg, BOOLEAN /* ignored*/ ) {
    // queue timer callback
    reaper *r = static_cast<reaper *>(arg);
    r->reap_check();
}

static void listener_begin_close(pn_listener_t* l);
static void connect_step_done(pconnection_t *pc, connect_result_t *result);
static pn_event_t *listener_batch_next(pn_event_batch_t *batch);
static pn_event_t *proactor_batch_next(pn_event_batch_t *batch);
static pn_event_t *pconnection_batch_next(pn_event_batch_t *batch);


static inline pn_proactor_t *batch_proactor(pn_event_batch_t *batch) {
  return (batch->next_event == proactor_batch_next) ?
    containerof(batch, pn_proactor_t, batch) : NULL;
}

static inline pn_listener_t *batch_listener(pn_event_batch_t *batch) {
  return (batch->next_event == listener_batch_next) ?
    containerof(batch, pn_listener_t, batch) : NULL;
}

static inline pconnection_t *batch_pconnection(pn_event_batch_t *batch) {
  return (batch->next_event == pconnection_batch_next) ?
    containerof(batch, pconnection_t, batch) : NULL;
}

static inline bool pconnection_has_event(pconnection_t *pc) {
  return pn_connection_driver_has_event(&pc->driver);
}

static inline bool listener_has_event(pn_listener_t *l) {
  return pn_collector_peek(l->collector);
}

static inline bool proactor_has_event(pn_proactor_t *p) {
  return pn_collector_peek(p->collector);
}

static void psocket_error_str(psocket_t *ps, const char *msg, const char* what) {
  if (ps->is_reaper)
    return;
  if (!ps->listener) {
    pn_connection_driver_t *driver = &as_pconnection_t(ps)->driver;
    pn_connection_driver_bind(driver); /* Bind so errors will be reported */
    pni_proactor_set_cond(pn_transport_condition(driver->transport), what, ps->host, ps->port, msg);
    pn_connection_driver_close(driver);
  } else {
    pn_listener_t *l = as_listener(ps);
    pni_proactor_set_cond(l->condition, what, ps->host, ps->port, msg);
    listener_begin_close(l);
  }
}

static void psocket_error(psocket_t *ps, int err, const char* what) {
  psocket_error_str(ps, errno_str2(err).c_str(), what);
}



// ========================================================================
// pconnection
// ========================================================================

/* Make a pn_class for pconnection_t since it is attached to a pn_connection_t record */
#define CID_pconnection CID_pn_object
#define pconnection_inspect NULL
#define pconnection_initialize NULL
#define pconnection_hashcode NULL
#define pconnection_compare NULL

static void pconnection_finalize(void *vp_pconnection) {
  pconnection_t *pc = (pconnection_t*)vp_pconnection;
  pcontext_finalize(&pc->context);
}

static const pn_class_t pconnection_class = PN_CLASS(pconnection);

static const char *pconnection_setup(pconnection_t *pc, pn_proactor_t *p, pn_connection_t *c, pn_transport_t *t, bool server, const char *addr)
{
  if (pn_connection_driver_init(&pc->driver, c, t) != 0) {
    free(pc);
    return "pn_connection_driver_init failure";
  }
  {
    csguard g(&p->bind_lock);
    pn_record_t *r = pn_connection_attachments(pc->driver.connection);
    if (pn_record_get(r, PN_PROACTOR)) {
      pn_connection_driver_destroy(&pc->driver);
      free(pc);
      return "pn_connection_t already in use";
    }
    pn_record_def(r, PN_PROACTOR, &pconnection_class);
    pn_record_set(r, PN_PROACTOR, pc);
    pc->bound = true;
    pc->can_wake = true;
  }

  pc->completion_queue = new std::queue<iocp_result_t *>();
  pc->work_queue = new std::queue<iocp_result_t *>();
  pcontext_init(&pc->context, PCONNECTION, p, pc);

  psocket_init(&pc->psocket, NULL, false, addr);
  pc->batch.next_event = pconnection_batch_next;
  if (server) {
    pn_transport_set_server(pc->driver.transport);
  }

  pn_decref(pc);                /* Will be deleted when the connection is */
  return NULL;
}

// Either stops a timer before firing or returns after the callback has
// completed (in the threadpool thread).  Never "in doubt".
static bool stop_timer(HANDLE tqueue, HANDLE *timer) {
  if (!*timer) return true;
  if (DeleteTimerQueueTimer(tqueue, *timer, INVALID_HANDLE_VALUE)) {
    *timer = NULL;
    return true;
  }
  return false;  // error
}

static bool start_timer(HANDLE tqueue, HANDLE *timer, WAITORTIMERCALLBACK cb, void *cb_arg, DWORD time) {
  if (*timer) {
    // TODO: log err
    return false;
  }
  return CreateTimerQueueTimer(timer, tqueue, cb, cb_arg, time, 0, WT_EXECUTEONLYONCE);
}

VOID CALLBACK tick_timer_cb(PVOID arg, BOOLEAN /* ignored*/ ) {
  pconnection_t *pc = (pconnection_t *) arg;
  csguard g(&pc->context.cslock);
  if (pc->psocket.iocpd && !pc->psocket.iocpd->closing) {
    pc->tick_pending = true;
    wakeup(&pc->psocket);
  }
}


// Call with no lock held or stop_timer and callback may deadlock
static void pconnection_tick(pconnection_t *pc) {
  pn_transport_t *t = pc->driver.transport;
  if (pn_transport_get_idle_timeout(t) || pn_transport_get_remote_idle_timeout(t)) {
    if(!stop_timer(pc->context.proactor->timer_queue, &pc->tick_timer)) {
      // TODO: handle error
    }
    uint64_t now = pn_proactor_now_64();
    uint64_t next = pn_transport_tick(t, now);
    if (next) {
      if (!start_timer(pc->context.proactor->timer_queue, &pc->tick_timer, tick_timer_cb, pc, next - now)) {
        // TODO: handle error
      }
    }
  }
}

static pconnection_t *get_pconnection(pn_connection_t* c) {
  if (!c) return NULL;
  pn_record_t *r = pn_connection_attachments(c);
  return (pconnection_t*) pn_record_get(r, PN_PROACTOR);
}


pn_listener_t *pn_event_listener(pn_event_t *e) {
  return (pn_event_class(e) == PN_CLASSCLASS(pn_listener)) ? (pn_listener_t*)pn_event_context(e) : NULL;
}

pn_proactor_t *pn_event_proactor(pn_event_t *e) {
  if (pn_event_class(e) == PN_CLASSCLASS(pn_proactor)) return (pn_proactor_t*)pn_event_context(e);
  pn_listener_t *l = pn_event_listener(e);
  if (l) return l->context.proactor;
  pn_connection_t *c = pn_event_connection(e);
  if (c) return pn_connection_proactor(pn_event_connection(e));
  return NULL;
}

// Call after successful accept
static void set_sock_names(pconnection_t *pc) {
  // This works.  Note possible use of GetAcceptExSockaddrs()
  pn_socket_t sock = pc->psocket.iocpd->socket;
  socklen_t len = sizeof(pc->local.ss);
  getsockname(sock, (struct sockaddr*)&pc->local.ss, &len);
  len = sizeof(pc->remote.ss);
  getpeername(sock, (struct sockaddr*)&pc->remote.ss, &len);
}


// Call with lock held when closing and transitioning away from working context
static inline bool pconnection_can_free(pconnection_t *pc) {
  return pc->psocket.iocpd == NULL && pc->context.completion_ops == 0
    && !pc->stop_timer_required && !pconnection_has_event(pc) && !pc->queued_disconnect;
}

static void pconnection_final_free(pconnection_t *pc) {
  if (pc->addrinfo) {
    freeaddrinfo(pc->addrinfo);
  }
  pn_condition_free(pc->disconnect_condition);
  pn_incref(pc);                /* Make sure we don't do a circular free */
  pn_connection_driver_destroy(&pc->driver);
  pn_decref(pc);
  /* Now pc is freed iff the connection is, otherwise remains till the pn_connection_t is freed. */
}

// Call with lock held or from forced shutdown
static void pconnection_begin_close(pconnection_t *pc) {
  if (!pc->context.closing) {
    pc->context.closing = true;
    pn_connection_driver_close(&pc->driver);
    pc->stop_timer_required = true;
    if (pc->context.proactor->reaper->add(pc->psocket.iocpd))
      pc->psocket.iocpd = NULL;
    wakeup(&pc->psocket);
  }
}

// call with lock held.  return true if caller must call pconnection_final_free()
static bool pconnection_cleanup(pconnection_t *pc) {
  delete pc->completion_queue;
  delete pc->work_queue;
  return proactor_remove(&pc->context);
}

static inline bool pconnection_work_pending(pconnection_t *pc) {
  if (pc->completion_queue->size() || pc->wake_count || pc->tick_pending || pc->queued_disconnect)
    return true;
  if (!pc->started)
    return false;
  pn_bytes_t wbuf = pn_connection_driver_write_buffer(&pc->driver);
  return (wbuf.size > 0 && (pc->psocket.iocpd->events & PN_WRITABLE));
}

// Return true unless reaped
static bool pconnection_write(pconnection_t *pc, pn_bytes_t wbuf) {
  ssize_t n;
  bool wouldblock;
  {
    csguard g(&pc->context.proactor->write_lock);
    n = pni_iocp_begin_write(pc->psocket.iocpd, wbuf.start, wbuf.size, &wouldblock, pc->psocket.iocpd->error);
  }
  if (n > 0) {
    pn_connection_driver_write_done(&pc->driver, n);
  } else if (n < 0 && !wouldblock) {
    psocket_error(&pc->psocket, WSAGetLastError(), "on write to");
  }
  else if (wbuf.size == 0 && pn_connection_driver_write_closed(&pc->driver)) {
    if (pc->context.proactor->reaper->add(pc->psocket.iocpd)) {
      pc->psocket.iocpd = NULL;
      return false;
    }
  }
  return true;
}

// Queue the result (if any) for the one worker thread.  Become the worker if possible.
// NULL result is a wakeup or a topup.
// topup signifies that the working thread is the caller looking for additional events.
static pn_event_batch_t *pconnection_process(pconnection_t *pc, iocp_result_t *result, bool topup) {
  bool first = true;
  bool waking = false;
  bool tick_required = false;
  bool open = false;

  while (true) {
    {
      csguard g(&pc->context.cslock);
      if (first) {
        first = false;
        if (result)
          pc->completion_queue->push(result);
        else if (!topup)
          wake_complete(&pc->context);
        if (!topup) {
          if (pc->context.working)
            return NULL;
          pc->context.working = true;
        }
        open = pc->started && !pc->connecting && !pc->context.closing;
      }
      else {
        // Just re-acquired lock after processing IO and engine work
        if (pconnection_has_event(pc))
          return &pc->batch;

        if (!pconnection_work_pending(pc)) {
          pc->context.working = false;
          if (pn_connection_driver_finished(&pc->driver)) {
            pconnection_begin_close(pc);
          }
          if (pc->context.closing && pconnection_can_free(pc)) {
            if (pconnection_cleanup(pc)) {
              g.release();
              pconnection_final_free(pc);
              return NULL;
            } // else disconnect logic has the free obligation
          }
          return NULL;
        }
      }

      if (pc->queued_disconnect) {  // From pn_proactor_disconnect()
        pc->queued_disconnect = false;
        if (!pc->context.closing) {
          if (pc->disconnect_condition) {
            pn_condition_copy(pn_transport_condition(pc->driver.transport), pc->disconnect_condition);
          }
          pn_connection_driver_close(&pc->driver);
        }
      }

      assert(pc->work_queue->empty());
      if (pc->completion_queue->size())
          std::swap(pc->work_queue, pc->completion_queue);

      if (pc->wake_count) {
        waking = open && pc->can_wake && !pn_connection_driver_finished(&pc->driver);
        pc->wake_count = 0;
      }
      if (pc->tick_pending) {
        pc->tick_pending = false;
        if (open)
          tick_required = true;
      }
    }

    // No lock

    // Drain completions prior to engine work
    while (pc->work_queue->size()) {
      result = (iocp_result_t *) pc->work_queue->front();
      pc->work_queue->pop();
      if (result->iocpd->closing) {
        if (pc->context.proactor->reaper->process(result)) {
          pc->psocket.iocpd = NULL;  // reaped
          open = false;
        }
      }
      else {
        if (is_write_result(result)) {
          csguard g(&pc->context.proactor->write_lock);
          do_complete(result);
        }
        else if (is_connect_result(result)) {
          connect_step_done(pc, (connect_result_t *) result);
          if (pc->psocket.iocpd && (pc->psocket.iocpd->events & PN_WRITABLE)) {
            pc->connecting = false;
            if (pc->started)
              open = true;
          }
        }
        else do_complete(result);
      }
    }

    if (!open) {
      if (pc->stop_timer_required) {
        pc->stop_timer_required = false;
        // Do without context lock to avoid possible deadlock
        stop_timer(pc->context.proactor->timer_queue, &pc->tick_timer);
      }
    } else {

      pn_rwbytes_t rbuf = pn_connection_driver_read_buffer(&pc->driver);
      pn_bytes_t wbuf = pn_connection_driver_write_buffer(&pc->driver);

      /* Ticks and checking buffers can generate events, process before proceeding */
      bool ready = pconnection_has_event(pc);
      if (waking) {
        pn_connection_t *c = pc->driver.connection;
        pn_collector_put(pn_connection_collector(c), PN_OBJECT, c, PN_CONNECTION_WAKE);
        waking = false;
      }
      if (ready) {
        continue;
      }

      if (wbuf.size >= 16384 && (pc->psocket.iocpd->events & PN_WRITABLE)) {
        if (!pconnection_write(pc, wbuf))
          continue;
      }
      if (rbuf.size > 0 && !pc->psocket.iocpd->read_in_progress) {
        bool wouldblock;
        ssize_t n = pni_iocp_recv(pc->psocket.iocpd, rbuf.start, rbuf.size, &wouldblock, pc->psocket.iocpd->error);

        if (n > 0) {
          pn_connection_driver_read_done(&pc->driver, n);
          pconnection_tick(pc);         /* check for tick changes. */
          tick_required = false;
        }
        else if (n == 0)
          pn_connection_driver_read_close(&pc->driver);
        else if (!wouldblock)
          psocket_error(&pc->psocket, WSAGetLastError(), "on read from");
      }
      if (!pc->psocket.iocpd->read_in_progress)
        start_reading(pc->psocket.iocpd);


      if (tick_required) {
        pconnection_tick(pc);         /* check for tick changes. */
        tick_required = false;
      }
      wbuf = pn_connection_driver_write_buffer(&pc->driver);
      if (wbuf.size > 0 && (pc->psocket.iocpd->events & PN_WRITABLE)) {
        if (!pconnection_write(pc, wbuf))
          open = false;
      }
    }

    if (topup)
      return NULL;  // regardless if new events made available
  }
}


static pn_event_t *pconnection_batch_next(pn_event_batch_t *batch) {
  pconnection_t *pc = batch_pconnection(batch);
  pn_event_t *e = pn_connection_driver_next_event(&pc->driver);
  if (!e && ++pc->hog_count < HOG_MAX) {
    pconnection_process(pc, NULL, true);  // top up
    e = pn_connection_driver_next_event(&pc->driver);
  }
  if (e && !pc->started && pn_event_type(e) == PN_CONNECTION_BOUND)
    pc->started = true; // SSL will be set up on return and safe to do IO with correct transport layers
  return e;
}

static void pconnection_done(pconnection_t *pc) {
  {
    csguard g(&pc->context.cslock);
    pc->context.working = false;
    pc->hog_count = 0;
    if (pconnection_has_event(pc) || pconnection_work_pending(pc)) {
      wakeup(&pc->psocket);
    } else if (pn_connection_driver_finished(&pc->driver)) {
      pconnection_begin_close(pc);
      wakeup(&pc->psocket);
    }
  }
}

static inline bool is_inactive(pn_proactor_t *p) {
  return (!p->contexts && !p->disconnects_pending && !p->timeout_set && !p->need_timeout && !p->shutting_down);
}

// Call whenever transitioning from "definitely active" to "maybe inactive"
static void wake_if_inactive(pn_proactor_t *p) {
  if (is_inactive(p)) {
    p->need_inactive = true;
    proactor_wake(p);
  }
}

void pn_proactor_done(pn_proactor_t *p, pn_event_batch_t *batch) {
  pconnection_t *pc = batch_pconnection(batch);
  if (pc) {
    pconnection_done(pc);
    return;
  }
  pn_listener_t *l = batch_listener(batch);
  if (l) {
    listener_done(l);
    return;
  }
  pn_proactor_t *bp = batch_proactor(batch);
  if (bp == p) {
    csguard g(&p->context.cslock);
    p->context.working = false;
    if (p->delayed_interrupt) {
      p->delayed_interrupt = false;
      p->need_interrupt = true;
    }
    if (p->timeout_processed) {
      p->timeout_processed = false;
      wake_if_inactive(p);
    }
    if (proactor_update_batch(p))
      proactor_wake(p);
    return;
  }
}

static void proactor_add_event(pn_proactor_t *p, pn_event_type_t t) {
  pn_collector_put(p->collector, PN_CLASSCLASS(pn_proactor), p, t);
}

static pn_event_batch_t *proactor_process(pn_proactor_t *p) {
  csguard g(&p->context.cslock);
  proactor_wake_complete(p);
  if (!p->context.working) {       /* Can generate proactor events */
    if (proactor_update_batch(p)) {
      p->context.working = true;
      return &p->batch;
    }
  }
  return NULL;
}

static pn_event_batch_t *psocket_process(psocket_t *ps, iocp_result_t *result, reaper *rpr) {
  if (ps) {
    pconnection_t *pc = as_pconnection_t(ps);
    if (pc) {
      return pconnection_process(pc, result, false);
    } else {
      pn_listener_t *l = as_listener(ps);
      if (l)
        return listener_process(l, result);
    }
  }
  rpr->process(result);
  return NULL;
}

static pn_event_batch_t *proactor_completion_loop(struct pn_proactor_t* p, bool can_block) {
  // Proact! Process inbound completions of async activity until one
  // of them provides a batch of events.
  while(true) {
    pn_event_batch_t *batch = NULL;

    DWORD win_timeout = can_block ? INFINITE : 0;
    DWORD num_xfer = 0;
    ULONG_PTR completion_key = 0;
    OVERLAPPED *overlapped = 0;

    bool good_op = GetQueuedCompletionStatus (p->iocp->completion_port, &num_xfer,
                                              &completion_key, &overlapped, win_timeout);
    if (!overlapped && !can_block && GetLastError() == WAIT_TIMEOUT)
      return NULL;  // valid timeout

    if (!good_op && !overlapped) {
      // Should never happen.  shutdown?
      // We aren't expecting a timeout, closed completion port, or other error here.
      PN_LOG_DEFAULT(PN_SUBSYSTEM_EVENT, PN_LEVEL_CRITICAL, "%s", errno_str("Windows Proton proactor internal failure\n", false).c_str());
      abort();
    }

    switch (completion_key) {
    case proactor_io: {
      // Normal IO case for connections and listeners
      iocp_result_t *result = (iocp_result_t *) overlapped;
      result->status = good_op ? 0 : GetLastError();
      result->num_transferred = num_xfer;
      psocket_t *ps = (psocket_t *) result->iocpd->active_completer;
      batch = psocket_process(ps, result, p->reaper);
      break;
    }
      // completion_key on our completion port is always null unless set by us
      // in PostQueuedCompletionStatus.  In which case, we hijack the overlapped
      // data structure for our own use.
    case psocket_wakeup_key:
        batch = psocket_process((psocket_t *) overlapped, NULL, p->reaper);
        break;
    case proactor_wake_key:
        batch = proactor_process((pn_proactor_t *) overlapped);
        break;
    case recycle_accept_key:
        recycle_result((accept_result_t *) overlapped);
        break;
    default:
        break;
    }
    if (batch) return batch;
    // No event generated.  Try again with next completion.
  }
}

pn_event_batch_t *pn_proactor_wait(struct pn_proactor_t* p) {
  return proactor_completion_loop(p, true);
}

pn_event_batch_t *pn_proactor_get(struct pn_proactor_t* p) {
  return proactor_completion_loop(p, false);
}

void pn_proactor_interrupt(pn_proactor_t *p) {
  csguard g(&p->context.cslock);
  if (p->context.working)
    p->delayed_interrupt = true;
  else
    p->need_interrupt = true;
  proactor_wake(p);
}

// runs on a threadpool thread.  Must not hold timer_lock.
VOID CALLBACK timeout_cb(PVOID arg, BOOLEAN /* ignored*/ ) {
  pn_proactor_t *p = (pn_proactor_t *) arg;
  csguard gtimer(&p->timer_lock);
  csguard g(&p->context.cslock);
  if (p->timeout_set)
    p->need_timeout = true;  // else cancelled
  p->timeout_set = false;
  if (p->need_timeout)
    proactor_wake(p);
}

void pn_proactor_set_timeout(pn_proactor_t *p, pn_millis_t t) {
  bool ticking = false;
  csguard gtimer(&p->timer_lock);
  {
    csguard g(&p->context.cslock);
    ticking = (p->timeout_timer != NULL);
    if (t == 0) {
      p->need_timeout = true;
      p->timeout_set = false;
      proactor_wake(p);
    }
    else
      p->timeout_set = true;
  }
  // Just timer_lock held
  if (ticking) {
    stop_timer(p->timer_queue, &p->timeout_timer);
  }
  if (t) {
    start_timer(p->timer_queue, &p->timeout_timer, timeout_cb, p, t);
  }
}

void pn_proactor_cancel_timeout(pn_proactor_t *p) {
  bool ticking = false;
  csguard gtimer(&p->timer_lock);
  {
    csguard g(&p->context.cslock);
    p->timeout_set = false;
    ticking = (p->timeout_timer != NULL);
  }
  if (ticking) {
    stop_timer(p->timer_queue, &p->timeout_timer);
    csguard g(&p->context.cslock);
    wake_if_inactive(p);
  }
}

// Return true if connect_step_done()will handle connection status
static bool connect_step(pconnection_t *pc) {
  pn_proactor_t *p = pc->context.proactor;
  while (pc->ai) {            /* Have an address */
    struct addrinfo *ai = pc->ai;
    pc->ai = pc->ai->ai_next; /* Move to next address in case this fails */
    unique_socket fd(::socket(ai->ai_family, SOCK_STREAM, ai->ai_protocol));
    if (fd != INVALID_SOCKET) {
      // Windows ConnectEx requires loosely bound socket.
      sockaddr_storage sa;
      memset(&sa, 0, sizeof(sa));
      sa.ss_family = ai->ai_family;
      if (!bind(fd, (SOCKADDR *) &sa, ai->ai_addrlen)) {
        pni_configure_sock_2(fd);
        pc->psocket.iocpd = pni_iocpdesc_create(p->iocp, fd);
        assert(pc->psocket.iocpd);
        pc->psocket.iocpd->write_closed = true;
        pc->psocket.iocpd->read_closed = true;
        fd.release();
        iocpdesc_t *iocpd = pc->psocket.iocpd;
        if (CreateIoCompletionPort ((HANDLE) iocpd->socket, iocpd->iocp->completion_port, proactor_io, 0)) {
          LPFN_CONNECTEX fn_connect_ex = lookup_connect_ex2(iocpd->socket);
          // addrinfo is owned by the pconnection so pass NULL to the connect result
          connect_result_t *result = connect_result(iocpd, NULL);
          iocpd->ops_in_progress++;
          iocpd->active_completer = &pc->psocket;
          // getpeername unreliable for outgoing connections, but we know it at this point
          memcpy(&pc->remote.ss, ai->ai_addr, ai->ai_addrlen);
          DWORD unused;
          bool success = fn_connect_ex(iocpd->socket, ai->ai_addr, ai->ai_addrlen,
                                       NULL, 0, &unused, (LPOVERLAPPED) result);
          if (success || WSAGetLastError() == ERROR_IO_PENDING) {
            return true;  // logic resumes at connect_step_done()
          } else {
            iocpd->ops_in_progress--;
            iocpd->active_completer = NULL;
            memset(&pc->remote.ss, 0, sizeof(pc->remote.ss));
          }
          pn_free(result);
        }
      }
      if (pc->psocket.iocpd) {
        pc->context.proactor->reaper->fast_reap(pc->psocket.iocpd);
        pc->psocket.iocpd = NULL;
      }
    }
  }
  pc->context.closing = true;
  return false;
}

static void connect_step_done(pconnection_t *pc, connect_result_t *result) {
  csguard g(&pc->context.cslock);
  DWORD saved_status = result->base.status;
  iocpdesc_t *iocpd = result->base.iocpd;
  iocpd->ops_in_progress--;
  assert(pc->psocket.iocpd == iocpd);
  complete_connect(result, result->base.status);  // frees result, starts regular IO if connected

  if (!saved_status) {
    // Success
    pc->psocket.iocpd->write_closed = false;
    pc->psocket.iocpd->read_closed = false;
    if (pc->addrinfo) {
      socklen_t len = sizeof(pc->local.ss);
      getsockname(pc->psocket.iocpd->socket, (struct sockaddr*)&pc->local.ss, &len);
      freeaddrinfo(pc->addrinfo);
      pc->addrinfo = NULL;
    }
    pc->ai = NULL;
    return;
  }
  else {
    // Descriptor will never be used.  Dispose.
    // Connect failed, no IO started, i.e. no pending iocpd based events
    pc->context.proactor->reaper->fast_reap(iocpd);
    pc->psocket.iocpd = NULL;
    memset(&pc->remote.ss, 0, sizeof(pc->remote.ss));
    // Is there a next connection target in the addrinfo to try?
    if (pc->ai && connect_step(pc)) {
      // Trying the next addrinfo possibility.  Will return here.
      return;
    }
    // Give up
    psocket_error(&pc->psocket, saved_status, "connect to ");
    pc->context.closing = true;
    wakeup(&pc->psocket);
  }
}

void pn_proactor_connect2(pn_proactor_t *p, pn_connection_t *c, pn_transport_t *t, const char *addr) {
  pconnection_t *pc = (pconnection_t*) pn_class_new(&pconnection_class, sizeof(pconnection_t));
  assert(pc); // TODO: memory safety
  const char *err = pconnection_setup(pc, p, c, t, false, addr);
  if (err) {
    PN_LOG_DEFAULT(PN_SUBSYSTEM_EVENT, PN_LEVEL_ERROR, "pn_proactor_connect failure: %s", err);
    return;
  }
  // TODO: check case of proactor shutting down
  csguard g(&pc->context.cslock);
  pc->connecting = true;
  proactor_add(&pc->context);
  pn_connection_open(pc->driver.connection); /* Auto-open */

  if (!pgetaddrinfo(pc->psocket.host, pc->psocket.port, 0, &pc->addrinfo)) {
    pc->ai = pc->addrinfo;
    if (connect_step(pc)) {
      return;
    }
  }
  psocket_error(&pc->psocket, WSAGetLastError(), "connect to ");
  wakeup(&pc->psocket);
  if (p->reaper->add(pc->psocket.iocpd)) {
    pc->psocket.iocpd = NULL;
  }
}

void pn_proactor_release_connection(pn_connection_t *c) {
  bool notify = false;
  pconnection_t *pc = get_pconnection(c);
  if (pc) {
    csguard g(&pc->context.cslock);
    // reverse lifecycle entanglement of pc and c from new_pconnection_t()
    pn_incref(pc);
    pn_proactor_t *p = pc->context.proactor;
    csguard g2(&p->bind_lock);
    pn_record_t *r = pn_connection_attachments(pc->driver.connection);
    pn_record_set(r, PN_PROACTOR, NULL);
    pn_connection_driver_release_connection(&pc->driver);
    pc->bound = false;  // Transport unbound
    g2.release();
    pconnection_begin_close(pc);
  }
}

void pn_proactor_listen(pn_proactor_t *p, pn_listener_t *l, const char *addr, int backlog)
{
  csguard g(&l->context.cslock);
  l->context.proactor = p;;
  l->backlog = backlog;
  proactor_add(&l->context);

//  l->overflow = NO_OVERFLOW;  TODO, as for epoll

  char addr_buf[PN_MAX_ADDR];
  const char *host, *port;
  pni_parse_addr(addr, addr_buf, PN_MAX_ADDR, &host, &port);

  struct addrinfo *addrinfo = NULL;
  int gai_err = pgetaddrinfo(host, port, AI_PASSIVE | AI_ALL, &addrinfo);
  int wsa_err = 0;
  if (!gai_err) {
    /* Count addresses, allocate enough space for sockets */
    size_t len = 0;
    for (struct addrinfo *ai = addrinfo; ai; ai = ai->ai_next) {
      ++len;
    }
    assert(len > 0);            /* guaranteed by getaddrinfo */
    l->psockets = (psocket_t*)calloc(len, sizeof(psocket_t));
    assert(l->psockets);      /* TODO: memory safety */
    l->psockets_size = 0;
    uint16_t dynamic_port = 0;  /* Record dynamic port from first bind(0) */
    /* Find working listen addresses */
    for (struct addrinfo *ai = addrinfo; ai; ai = ai->ai_next) {
      if (dynamic_port) set_port(ai->ai_addr, dynamic_port);
      // Note fd destructor can clear WSAGetLastError()
      unique_socket fd(::socket(ai->ai_family, SOCK_STREAM, ai->ai_protocol));
      if (fd != INVALID_SOCKET) {
        bool yes = 1;
        if (!::bind(fd, ai->ai_addr, ai->ai_addrlen))
          if (!::listen(fd, backlog)) {
            iocpdesc_t *iocpd = pni_iocpdesc_create(p->iocp, fd);
            if (iocpd) {
              pn_socket_t sock = fd.release();
              psocket_t *ps = &l->psockets[l->psockets_size++];
              psocket_init(ps, l, false, addr);
              ps->iocpd = iocpd;
              iocpd->is_mp = true;
              iocpd->active_completer = ps;
              pni_iocpdesc_start(ps->iocpd);
              /* Get actual address */
              socklen_t len = sizeof(ps->listen_addr.ss);
              (void)getsockname(sock, (struct sockaddr*)&ps->listen_addr.ss, &len);
              if (ps == l->psockets) { /* First socket, check for dynamic port */
                dynamic_port = check_dynamic_port(ai->ai_addr, pn_netaddr_sockaddr(&ps->listen_addr));
              } else {
                (ps-1)->listen_addr.next = &ps->listen_addr; /* Link into list */
              }
            }
          }
      }
      wsa_err = WSAGetLastError();  // save it
    }
  }
  if (addrinfo) {
    freeaddrinfo(addrinfo);
  }
  if (l->psockets_size == 0) { /* All failed, create dummy socket with an error */
    l->psockets = (psocket_t*)calloc(sizeof(psocket_t), 1);
    psocket_init(l->psockets, l, false, addr);
    if (gai_err) {
      psocket_error(l->psockets, gai_err, "listen on");
    } else {
      psocket_error(l->psockets, wsa_err, "listen on");
    }
  } else {
    pn_collector_put(l->collector, PN_CLASSCLASS(pn_listener), l, PN_LISTENER_OPEN);
  }
  wakeup(l->psockets);
}

// Assumes listener lock is held
static pn_event_batch_t *batch_owned(pn_listener_t *l) {
  if (l->close_dispatched) return NULL;
  if (!l->context.working) {
    if (listener_has_event(l)) {
      l->context.working = true;
      return &l->batch;
    }
    assert(!(l->context.closing && l->pending_events));
    if (l->pending_events) {
      pn_collector_put(l->collector, PN_CLASSCLASS(pn_listener), l, PN_LISTENER_ACCEPT);
      l->pending_events--;
      l->context.working = true;
      return &l->batch;
    }
  }
  return NULL;
}

static void listener_close_all(pn_listener_t *l) {
  for (size_t i = 0; i < l->psockets_size; ++i) {
    psocket_t *ps = &l->psockets[i];
    if(ps->iocpd && !ps->iocpd->closing)
      if (l->context.proactor->reaper->add(ps->iocpd))
        ps->iocpd = NULL;
  }
}

static bool listener_can_free(pn_listener_t *l) {
  if (!l->close_dispatched) return false;
  if (l->context.working || l->context.completion_ops) return false;
  for (size_t i = 0; i < l->psockets_size; ++i) {
    psocket_t *ps = &l->psockets[i];
    if (ps->iocpd)
      return false;
  }
  return true;
}

/* Call with lock not held */
static inline void listener_final_free(pn_listener_t *l) {
  pcontext_finalize(&l->context);
  free(l->psockets);
  if (l->collector) pn_collector_free(l->collector);
  if (l->condition) pn_condition_free(l->condition);
  if (l->attachments) pn_free(l->attachments);
  free(l);
}

/* Call with listener lock held by lg.*/
static void internal_listener_free(pn_listener_t *l, csguard &g) {
  bool can_free = true;
  if (l->context.proactor) {
    can_free = proactor_remove(&l->context);
  }
  g.release();
  if (can_free)
    listener_final_free(l);
  // else final free is done by proactor_disconnect()
}

static bool listener_maybe_free(pn_listener_t *l, csguard &g) {
  if (listener_can_free(l)) {
    internal_listener_free(l, g);
    return true;
  }
  return false;
}

static pn_event_batch_t *listener_process(pn_listener_t *l, iocp_result_t *result) {
  accept_result_t *accept_result = NULL;
  psocket_t *ps = NULL;
  {
    csguard g(&l->context.cslock);
    if (!result) {
      wake_complete(&l->context);
      if (listener_maybe_free(l, g)) return NULL;
      return batch_owned(l);
    }
    else
      ps = (psocket_t *) result->iocpd->active_completer;

    if (!l->context.closing && result->status) {
      psocket_error(ps, WSAGetLastError(), "listen on "); // initiates close/multi-reap
    }
    if (l->context.closing) {
      if (l->context.proactor->reaper->process(result)) {
        ps->iocpd = NULL;
        if (listener_maybe_free(l, g)) return NULL;
      }
      return batch_owned(l);
    }

    accept_result = (accept_result_t *) result;
    l->context.completion_ops++;  // prevent accidental deletion while lock is temporarily removed
  }

  // No lock

  pn_socket_t accept_sock = accept_result->new_sock->socket;
  // AcceptEx special setsockopt:
  setsockopt(accept_sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&ps->iocpd->socket,
             sizeof (SOCKET));
  // Connected.
  iocpdesc_t *conn_iocpd = accept_result->new_sock;
  conn_iocpd->read_closed = false;
  conn_iocpd->write_closed = false;
  pni_configure_sock_2(conn_iocpd->socket);

  {
    csguard g(&l->context.cslock);
    l->context.completion_ops--;
    accept_result->new_sock->ops_in_progress--;
    ps->iocpd->ops_in_progress--;

    // add even if closing to reuse cleanup code
    l->pending_accepts->push(accept_result);
    if (!ps->iocpd->ops_in_progress)
      begin_accept(ps->iocpd->acceptor, NULL);  // Start another, up to IOCP_MAX_ACCEPTS
    l->pending_events++;
    if (l->context.closing)
      release_pending_accepts(l);
    if (listener_maybe_free(l, g)) return NULL;
    return batch_owned(l);
  }
}

pn_proactor_t *pn_connection_proactor(pn_connection_t* c) {
  pconnection_t *pc = get_pconnection(c);
  return pc ? pc->context.proactor : NULL;
}

void pn_connection_wake(pn_connection_t* c) {
  pconnection_t *pc = get_pconnection(c);
  csguard g(&pc->context.cslock);
  if (!pc->context.closing) {
    pc->wake_count++;
    wakeup(&pc->psocket);
  }
}

pn_proactor_t *pn_proactor() {
  HANDLE tq = NULL;
  bool wsa = false;
  iocp_t *iocp = NULL;
  pn_proactor_t *p = NULL;
  pn_collector_t *c = NULL;
  class reaper *r = NULL;

  tq = CreateTimerQueue();
  if (tq) {
    p = (pn_proactor_t*)calloc(1, sizeof(*p));
    c = pn_collector();
    if (p && c) {
      /* Request WinSock 2.2 */
      WORD wsa_ver = MAKEWORD(2, 2);
      WSADATA unused;
      if (!WSAStartup(wsa_ver, &unused)) {
        wsa = true;
        if (iocp = pni_iocp()) {
          InitializeCriticalSectionAndSpinCount(&p->context.cslock, 4000);
          InitializeCriticalSectionAndSpinCount(&p->write_lock, 4000);
          InitializeCriticalSectionAndSpinCount(&p->timer_lock, 4000);
          InitializeCriticalSectionAndSpinCount(&p->bind_lock, 4000);
          try {
            r = new reaper(p, &p->write_lock, iocp);
            // success
            p->iocp = iocp;
            p->reaper = r;
            p->batch.next_event = &proactor_batch_next;
            p->collector = c;
            p->timer_queue = tq;
            return p;
          } catch (...) {}
        }
      }
    }
  }
  fprintf(stderr, "%s\n", errno_str("Windows Proton proactor OS resource failure", false).c_str());
  if (iocp) pn_free((void *) iocp);
  if (wsa) WSACleanup();
  free(c);
  free(p);
  if (tq) DeleteTimerQueueEx(tq, NULL);
  return NULL;
}

void pn_proactor_free(pn_proactor_t *p) {
  DeleteTimerQueueEx(p->timer_queue, INVALID_HANDLE_VALUE);
  DeleteCriticalSection(&p->timer_lock);
  DeleteCriticalSection(&p->bind_lock);
  proactor_shutdown(p);

  delete p->reaper;
  WSACleanup();
  pn_collector_free(p->collector);
  free(p);
}

static pn_event_t *listener_batch_next(pn_event_batch_t *batch) {
  pn_listener_t *l = batch_listener(batch);
  {
    csguard g(&l->context.cslock);
    if (!listener_has_event(l) && l->pending_events) {
      pn_collector_put(l->collector, PN_CLASSCLASS(pn_listener), l, PN_LISTENER_ACCEPT);
      l->pending_events--;
    }
    pn_event_t *e = pn_collector_next(l->collector);
    if (e && pn_event_type(e) == PN_LISTENER_CLOSE)
      l->close_dispatched = true;
    return pni_log_event(l, e);
  }
}

static void listener_done(pn_listener_t *l) {
  {
    csguard g(&l->context.cslock);
    l->context.working = false;
    if (l->close_dispatched) {
      listener_maybe_free(l, g);
      return;
    }
    else
      if (listener_has_event(l))
        wakeup(l->psockets);
  }
}

pn_listener_t *pn_listener() {
  pn_listener_t *l = (pn_listener_t*)calloc(1, sizeof(pn_listener_t));
  if (l) {
    l->batch.next_event = listener_batch_next;
    l->collector = pn_collector();
    l->condition = pn_condition();
    l->attachments = pn_record();
    l->pending_accepts = new std::queue<accept_result_t *>();
    if (!l->condition || !l->collector || !l->attachments) {
      pn_listener_free(l);
      return NULL;
    }
    pn_proactor_t *unknown = NULL;  // won't know until pn_proactor_listen
    pcontext_init(&l->context, LISTENER, unknown, l);
  }
  return l;
}

void pn_listener_free(pn_listener_t *l) {
  /* Note at this point either the listener has never been used (freed
     by user) or it has been closed, and all pending operations
     completed, i.e. listener_can_free() is true.
  */
  if (l) {
    csguard g(&l->context.cslock);
    if (l->context.proactor) {
      internal_listener_free(l, g);
      return;
    }
    // freed by user
    g.release();
    listener_final_free(l);
  }
}

static void listener_begin_close(pn_listener_t* l) {
  if (l->context.closing)
    return;
  l->context.closing = true;
  listener_close_all(l);
  release_pending_accepts(l);
  pn_collector_put(l->collector, PN_CLASSCLASS(pn_listener), l, PN_LISTENER_CLOSE);
}

void pn_listener_close(pn_listener_t* l) {
  csguard g(&l->context.cslock);
  listener_begin_close(l);
  wakeup(&l->psockets[0]);
}

pn_proactor_t *pn_listener_proactor(pn_listener_t* l) {
  return l ? l->context.proactor : NULL;
}

pn_condition_t* pn_listener_condition(pn_listener_t* l) {
  return l->condition;
}

void *pn_listener_get_context(pn_listener_t *l) {
  return l->listener_context;
}

void pn_listener_set_context(pn_listener_t *l, void *context) {
  l->listener_context = context;
}

pn_record_t *pn_listener_attachments(pn_listener_t *l) {
  return l->attachments;
}

static void release_pending_accepts(pn_listener_t *l) {
  // called with lock held or at shutdown
  while (!l->pending_accepts->empty()) {
    accept_result_t *accept_result = l->pending_accepts->front();
    l->pending_accepts->pop();
    psocket_t *ps = (psocket_t *) accept_result->base.iocpd->active_completer;
    accept_result->new_sock->ops_in_progress--;
    ps->iocpd->ops_in_progress--;
    l->context.proactor->reaper->add(accept_result->new_sock);
    begin_accept(accept_result->base.iocpd->acceptor, accept_result); // does proper disposal when closing
  }
  l->pending_events = 0;
}

static void recycle_result(accept_result_t *accept_result) {
  psocket_t *ps = (psocket_t *) accept_result->base.iocpd->active_completer;
  pn_listener_t *l = ps->listener;
  reset_accept_result(accept_result);
  accept_result->new_sock = create_same_type_socket(ps->iocpd);
  {
    csguard g(&l->context.cslock);
    if (l->context.closing && accept_result->new_sock) {
      closesocket(accept_result->new_sock->socket);
      accept_result->new_sock->socket = INVALID_SOCKET;
    }
    begin_accept(ps->iocpd->acceptor, accept_result);  // cleans up if closing
    l->context.completion_ops--;
    if (l->context.closing && listener_maybe_free(l, g))
      return;
  }
}

void pn_listener_accept2(pn_listener_t *l, pn_connection_t *c, pn_transport_t *t) {
  accept_result_t *accept_result = NULL;
  DWORD err = 0;
  psocket_t *ps = NULL;
  pn_proactor_t *p = l->context.proactor;

  {
    csguard g(&l->context.cslock);
    pconnection_t *pc = (pconnection_t*) pn_class_new(&pconnection_class, sizeof(pconnection_t));
    assert(pc);  // TODO: memory safety
    const char *err_str = pconnection_setup(pc, p, c, t, true, "");
    if (err_str) {
      PN_LOG_DEFAULT(PN_SUBSYSTEM_EVENT, PN_LEVEL_ERROR, "pn_listener_accept failure: %s", err_str);
      return;
    }
    proactor_add(&pc->context);

    if (l->context.closing)
      err = WSAESHUTDOWN;
    else if (l->pending_accepts->empty())
      err = WSAEWOULDBLOCK;
    else {
      accept_result = l->pending_accepts->front();
      l->pending_accepts->pop();
      ps = (psocket_t *) accept_result->base.iocpd->active_completer;
      l->context.completion_ops++; // for recycle_result
      iocpdesc_t *conn_iocpd = accept_result->new_sock;
      pc->psocket.iocpd = conn_iocpd;
      conn_iocpd->active_completer =&pc->psocket;
      set_sock_names(pc);
      pc->started = true;
      csguard g(&pc->context.cslock);
      pni_iocpdesc_start(conn_iocpd);
    }

    if (err) {
      psocket_error(&pc->psocket, err, "listen on");
      wakeup(&pc->psocket);
    }
  }
  // Use another thread to prepare a replacement async accept request
  post_completion(p->iocp, recycle_accept_key, accept_result);
}



// Call with lock held.  Leave unchanged if events pending.
// Return true if there is an event in the collector
static bool proactor_update_batch(pn_proactor_t *p) {
  if (proactor_has_event(p))
    return true;
  if (p->need_timeout) {
    p->need_timeout = false;
    proactor_add_event(p, PN_PROACTOR_TIMEOUT);
    return true;
  }
  if (p->need_interrupt) {
    p->need_interrupt = false;
    proactor_add_event(p, PN_PROACTOR_INTERRUPT);
    return true;
  }
  if (p->need_inactive) {
    p->need_inactive = false;
    proactor_add_event(p, PN_PROACTOR_INACTIVE);
    return true;
  }
  return false;
}

static pn_event_t *proactor_batch_next(pn_event_batch_t *batch) {
  pn_proactor_t *p = batch_proactor(batch);
  pn_event_t *e = pn_collector_next(p->collector);
  if (!e) {
    csguard g(&p->context.cslock);
    proactor_update_batch(p);
    e = pn_collector_next(p->collector);
  }
  if (e && pn_event_type(e) == PN_PROACTOR_TIMEOUT)
    p->timeout_processed = true;
  return pni_log_event(p, e);
}

static void proactor_add(pcontext_t *ctx) {
  pn_proactor_t *p = ctx->proactor;
  csguard g(&p->context.cslock);
  if (p->contexts) {
    p->contexts->prev = ctx;
    ctx->next = p->contexts;
  }
  p->contexts = ctx;
}

// call with pcontext lock held
static bool proactor_remove(pcontext_t *ctx) {
  pn_proactor_t *p = ctx->proactor;
  csguard g(&p->context.cslock);
  bool can_free = true;
  if (ctx->disconnecting) {
    // No longer on contexts list
    --p->disconnects_pending;
    if (--ctx->disconnect_ops != 0) {
      // proactor_disconnect() does the free
      can_free = false;
    }
  }
  else {
    // normal case
    if (ctx->prev)
      ctx->prev->next = ctx->next;
    else {
      p->contexts = ctx->next;
      ctx->next = NULL;
      if (p->contexts)
        p->contexts->prev = NULL;
    }
    if (ctx->next) {
      ctx->next->prev = ctx->prev;
    }
  }
  wake_if_inactive(p);
  return can_free;
}

static void pconnection_forced_shutdown(pconnection_t *pc) {
  // Called by proactor_free, no competing threads processing iocp activity.
  pconnection_begin_close(pc);
  // Timer threads may lurk. No lock held, so no deadlock risk
  stop_timer(pc->context.proactor->timer_queue, &pc->tick_timer);
  pconnection_final_free(pc);
}

static void listener_forced_shutdown(pn_listener_t *l) {
  // Called by proactor_free, no competing threads, no iocp activity.
  listener_close_all(l);  // reaper gets all iocp descriptors to cleanup
  listener_final_free(l);
}


static void proactor_shutdown(pn_proactor_t *p) {
  // Called from free(), no competing threads except our own timer queue callbacks.
  p->shutting_down = true;
  while (p->contexts) {
    pcontext_t *ctx = p->contexts;
    p->contexts = ctx->next;
    switch (ctx->type) {
     case PCONNECTION:
      pconnection_forced_shutdown(pcontext_pconnection(ctx));
      break;
     case LISTENER:
      listener_forced_shutdown(pcontext_listener(ctx));
      break;
     default:
      break;
    }
  }

  // Graceful close of the sockets (zombies), closes completion port, free iocp related resources
  p->reaper->final_shutdown();
}

void pn_proactor_disconnect(pn_proactor_t *p, pn_condition_t *cond) {
  pcontext_t *disconnecting_pcontexts = NULL;
  pcontext_t *ctx = NULL;
  {
    csguard g(&p->context.cslock);
    // Move the whole contexts list into a disconnecting state
    disconnecting_pcontexts = p->contexts;
    p->contexts = NULL;
    // First pass: mark each pcontext as disconnecting and update global pending count.
    pcontext_t *ctx = disconnecting_pcontexts;
    while (ctx) {
      ctx->disconnecting = true;
      ctx->disconnect_ops = 2;   // Second pass below and proactor_remove(), in any order.
      p->disconnects_pending++;
      ctx = ctx->next;
    }
  }
  // no lock

  if (!disconnecting_pcontexts) {
    csguard p_guard(&p->context.cslock);
    wake_if_inactive(p);
    return;
  }

  // Second pass: different locking, close the pcontexts, free them if !disconnect_ops
  pcontext_t *next = disconnecting_pcontexts;
  while (next) {
    ctx = next;
    next = ctx->next;           /* Save next pointer in case we free ctx */
    bool do_free = false;
    pconnection_t *pc = pcontext_pconnection(ctx);
    pn_listener_t *l = pc ? NULL : pcontext_listener(ctx);
    CRITICAL_SECTION *ctx_cslock = pc ? &pc->context.cslock : &l->context.cslock;
    csguard ctx_guard(ctx_cslock);
    if (pc) {
      pc->can_wake = false;
      if (!ctx->closing) {
        if (ctx->working) {
          // Must defer
          pc->queued_disconnect = true;
          if (cond) {
            if (!pc->disconnect_condition)
              pc->disconnect_condition = pn_condition();
            pn_condition_copy(pc->disconnect_condition, cond);
          }
        }
        else {
          // No conflicting working context.
          if (cond) {
            pn_condition_copy(pn_transport_condition(pc->driver.transport), cond);
          }
          pn_connection_driver_close(&pc->driver);
        }
      }
    } else {
      assert(l);
      if (!ctx->closing) {
        if (cond) {
          pn_condition_copy(pn_listener_condition(l), cond);
        }
        listener_begin_close(l);
      }
    }

    csguard p_guard(&p->context.cslock);
    if (--ctx->disconnect_ops == 0) {
      do_free = true;
      wake_if_inactive(p);
    } else {
      // If initiating the close, wake the pcontext to do the free.
      wakeup(pc ? &pc->psocket : l->psockets);
    }
    p_guard.release();
    ctx_guard.release();

    if (do_free) {
      if (pc) pconnection_final_free(pc);
      else listener_final_free(pcontext_listener(ctx));
    }
  }
}

const pn_netaddr_t *pn_transport_local_addr(pn_transport_t *t) {
  pconnection_t *pc = get_pconnection(pn_transport_connection(t));
  return pc? &pc->local : NULL;
}

const pn_netaddr_t *pn_transport_remote_addr(pn_transport_t *t) {
  pconnection_t *pc = get_pconnection(pn_transport_connection(t));
  return pc ? &pc->remote : NULL;
}

const pn_netaddr_t *pn_listener_addr(pn_listener_t *l) {
  return l->psockets ? &l->psockets[0].listen_addr : NULL;
}

pn_millis_t pn_proactor_now(void) {
    return (pn_millis_t) pn_proactor_now_64();
}

int64_t pn_proactor_now_64(void) {
  return GetTickCount64();
}

// Empty stubs for raw connection code
pn_raw_connection_t *pn_raw_connection(void) { return NULL; }
void pn_proactor_raw_connect(pn_proactor_t *p, pn_raw_connection_t *rc, const char *addr) {}
void pn_listener_raw_accept(pn_listener_t *l, pn_raw_connection_t *rc) {}
void pn_raw_connection_wake(pn_raw_connection_t *conn) {}
void pn_raw_connection_close(pn_raw_connection_t *conn) {}
void pn_raw_connection_read_close(pn_raw_connection_t *conn) {}
void pn_raw_connection_write_close(pn_raw_connection_t *conn) {}
const struct pn_netaddr_t *pn_raw_connection_local_addr(pn_raw_connection_t *connection) { return NULL; }
const struct pn_netaddr_t *pn_raw_connection_remote_addr(pn_raw_connection_t *connection) { return NULL; }
