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

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#if _WIN32_WINNT < 0x0501
#error "Proton requires Windows API support for XP or later."
#endif
#include <winsock2.h>
#include <mswsock.h>
#include <Ws2tcpip.h>
#define PN_WINAPI

#include "platform.h"
#include <proton/object.h>
#include <proton/io.h>
#include <proton/selector.h>
#include <proton/error.h>
#include <proton/transport.h>
#include "iocp.h"
#include "util.h"
#include <assert.h>

/*
 * Windows IO Completion Port support for Proton.
 *
 * Overlapped writes are used to avoid lengthy stalls between write
 * completion and starting a new write.  Non-overlapped reads are used
 * since Windows accumulates inbound traffic without stalling and
 * managing read buffers would not avoid a memory copy at the pn_read
 * boundary.
 */

// Max number of overlapped accepts per listener
#define IOCP_MAX_ACCEPTS 10

// AcceptEx squishes the local and remote addresses and optional data
// all together when accepting the connection. Reserve enough for
// IPv6 addresses, even if the socket is IPv4. The 16 bytes padding
// per address is required by AcceptEx.
#define IOCP_SOCKADDRMAXLEN (sizeof(sockaddr_in6) + 16)
#define IOCP_SOCKADDRBUFLEN (2 * IOCP_SOCKADDRMAXLEN)

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
static void start_reading(iocpdesc_t *iocpd);
static bool is_listener(iocpdesc_t *iocpd);
static void release_sys_sendbuf(SOCKET s);

static void iocpdesc_fail(iocpdesc_t *iocpd, HRESULT status, const char* text)
{
  pni_win32_error(iocpd->error, text, status);
  if (iocpd->iocp->iocp_trace) {
    iocp_log("connection terminated: %s\n", pn_error_text(iocpd->error));
  }
  if (!is_listener(iocpd) && !iocpd->write_closed && !pni_write_pipeline_size(iocpd->pipeline))
    iocp_shutdown(iocpd);
  iocpd->write_closed = true;
  iocpd->read_closed = true;
  pni_events_update(iocpd, iocpd->events | PN_READABLE | PN_WRITABLE);
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
static iocpdesc_t *create_same_type_socket(iocpdesc_t *iocpd)
{
  sockaddr_storage sa;
  socklen_t salen = sizeof(sa);
  if (getsockname(iocpd->socket, (sockaddr*)&sa, &salen) == -1)
    return NULL;
  SOCKET s = socket(sa.ss_family, SOCK_STREAM, 0); // Currently only work with SOCK_STREAM
  if (s == INVALID_SOCKET)
    return NULL;
  return pni_iocpdesc_create(iocpd->iocp, s, false);
}

static bool is_listener(iocpdesc_t *iocpd)
{
  return iocpd && iocpd->acceptor;
}

// === Async accept processing

typedef struct {
  iocp_result_t base;
  iocpdesc_t *new_sock;
  char address_buffer[IOCP_SOCKADDRBUFLEN];
  DWORD unused;
} accept_result_t;

static accept_result_t *accept_result(iocpdesc_t *listen_sock) {
  accept_result_t *result = (accept_result_t *)calloc(1, sizeof(accept_result_t));
  if (result) {
    result->base.type = IOCP_ACCEPT;
    result->base.iocpd = listen_sock;
  }
  return result;
}

static void reset_accept_result(accept_result_t *result) {
  memset(&result->base.overlapped, 0, sizeof (OVERLAPPED));
  memset(&result->address_buffer, 0, IOCP_SOCKADDRBUFLEN);
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

static void begin_accept(pni_acceptor_t *acceptor, accept_result_t *result)
{
  if (acceptor->listen_sock->closing) {
    if (result) {
      free(result);
      acceptor->accept_queue_size--;
    }
    if (acceptor->accept_queue_size == 0)
      acceptor->signalled = true;
    return;
  }

  if (result) {
    reset_accept_result(result);
  } else {
    if (acceptor->accept_queue_size < IOCP_MAX_ACCEPTS &&
        pn_list_size(acceptor->accepts) == acceptor->accept_queue_size ) {
      result = accept_result(acceptor->listen_sock);
      acceptor->accept_queue_size++;
    } else {
      // an async accept is still pending or max concurrent accepts already hit
      return;
    }
  }

  result->new_sock = create_same_type_socket(acceptor->listen_sock);
  if (result->new_sock) {
    // Not yet connected.
    result->new_sock->read_closed = true;
    result->new_sock->write_closed = true;

    bool success = acceptor->fn_accept_ex(acceptor->listen_sock->socket, result->new_sock->socket,
                     result->address_buffer, 0, IOCP_SOCKADDRMAXLEN, IOCP_SOCKADDRMAXLEN,
                     &result->unused, (LPOVERLAPPED) result);
    if (!success && WSAGetLastError() != ERROR_IO_PENDING) {
      result->base.status = WSAGetLastError();
      pn_list_add(acceptor->accepts, result);
      pni_events_update(acceptor->listen_sock, acceptor->listen_sock->events | PN_READABLE);
    } else {
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
    free(result);    // discard
    reap_check(ld);
  } else {
    result->base.status = status;
    pn_list_add(ld->acceptor->accepts, result);
    pni_events_update(ld, ld->events | PN_READABLE);
  }
}

pn_socket_t pni_iocp_end_accept(iocpdesc_t *ld, sockaddr *addr, socklen_t *addrlen, bool *would_block, pn_error_t *error)
{
  if (!is_listener(ld)) {
    set_iocp_error_status(error, PN_ERR, WSAEOPNOTSUPP);
    return INVALID_SOCKET;
  }
  if (ld->read_closed) {
    set_iocp_error_status(error, PN_ERR, WSAENOTSOCK);
    return INVALID_SOCKET;
  }
  if (pn_list_size(ld->acceptor->accepts) == 0) {
    if (ld->events & PN_READABLE && ld->iocp->iocp_trace)
      iocp_log("listen socket readable with no available accept completions\n");
    *would_block = true;
    return INVALID_SOCKET;
  }

  accept_result_t *result = (accept_result_t *) pn_list_get(ld->acceptor->accepts, 0);
  pn_list_del(ld->acceptor->accepts, 0, 1);
  if (!pn_list_size(ld->acceptor->accepts))
    pni_events_update(ld, ld->events & ~PN_READABLE);  // No pending accepts

  pn_socket_t accept_sock;
  if (result->base.status) {
    accept_sock = INVALID_SOCKET;
    pni_win32_error(ld->error, "accept failure", result->base.status);
    if (ld->iocp->iocp_trace)
      iocp_log("%s\n", pn_error_text(ld->error));
    // App never sees this socket so close it here.
    pni_iocp_begin_close(result->new_sock);
  } else {
    accept_sock = result->new_sock->socket;
    // AcceptEx special setsockopt:
    setsockopt(accept_sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&ld->socket,
                  sizeof (SOCKET));
    if (addr && addrlen && *addrlen > 0) {
      sockaddr_storage *local_addr = NULL;
      sockaddr_storage *remote_addr = NULL;
      int local_addrlen, remote_addrlen;
      LPFN_GETACCEPTEXSOCKADDRS fn = ld->acceptor->fn_get_accept_ex_sockaddrs;
      fn(result->address_buffer, 0, IOCP_SOCKADDRMAXLEN, IOCP_SOCKADDRMAXLEN,
         (SOCKADDR **) &local_addr, &local_addrlen, (SOCKADDR **) &remote_addr,
         &remote_addrlen);
      *addrlen = pn_min(*addrlen, remote_addrlen);
      memmove(addr, remote_addr, *addrlen);
    }
  }

  if (accept_sock != INVALID_SOCKET) {
    // Connected.
    result->new_sock->read_closed = false;
    result->new_sock->write_closed = false;
  }

  // Done with the completion result, so reuse it
  result->new_sock = NULL;
  begin_accept(ld->acceptor, result);
  return accept_sock;
}


// === Async connect processing

typedef struct {
  iocp_result_t base;
  char address_buffer[IOCP_SOCKADDRBUFLEN];
  struct addrinfo *addrinfo;
} connect_result_t;

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

static connect_result_t *connect_result(iocpdesc_t *iocpd, struct addrinfo *addr) {
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

  iocpdesc_t *iocpd = pni_iocpdesc_create(iocp, sock, false);
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
    pni_iocp_begin_close(iocpd);
    sock = INVALID_SOCKET;
    if (iocp->iocp_trace)
      iocp_log("%s\n", pn_error_text(error));
  } else {
    iocpd->ops_in_progress++;
  }
  return sock;
}

static void complete_connect(connect_result_t *result, HRESULT status)
{
  iocpdesc_t *iocpd = result->base.iocpd;
  if (iocpd->closing) {
    pn_free(result);
    reap_check(iocpd);
    return;
  }

  if (status) {
    iocpdesc_fail(iocpd, status, "Connect failure");
  } else {
    release_sys_sendbuf(iocpd->socket);
    if (setsockopt(iocpd->socket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT,  NULL, 0)) {
      iocpdesc_fail(iocpd, WSAGetLastError(), "Connect failure (update context)");
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
    ssize_t actual_len = pn_min(len, result->buffer.size);
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

static void complete_write(write_result_t *result, DWORD xfer_count, HRESULT status)
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
  pni_write_pipeline_return(iocpd->pipeline, result);
  iocpdesc_fail(iocpd, status, "IOCP async write error");
}


// === Async reads

struct read_result_t {
  iocp_result_t base;
  size_t drain_count;
  char unused_buf[1];
};

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
  closesocket(iocpd->socket);
  iocpd->socket = INVALID_SOCKET;
}


static void complete_read(read_result_t *result, DWORD xfer_count, HRESULT status)
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

  size_t count = recv(iocpd->socket, (char *) buf, size, 0);
  if (count > 0) {
    pni_events_update(iocpd, iocpd->events & ~PN_READABLE);
    begin_zero_byte_read(iocpd);
    return count;
  } else if (count == 0) {
    iocpd->read_closed = true;
    return 0;
  }
  if (WSAGetLastError() == WSAEWOULDBLOCK)
    *would_block = true;
  else
    set_iocp_error_status(error, PN_ERR, WSAGetLastError());
  return SOCKET_ERROR;
}

static void start_reading(iocpdesc_t *iocpd)
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
  assert (s != INVALID_SOCKET);
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

iocpdesc_t *pni_iocpdesc_create(iocp_t *iocp, pn_socket_t s, bool external) {
  assert(!pni_iocpdesc_map_get(iocp, s));
  bool listening = is_listener_socket(s);
  iocpdesc_t *iocpd = pni_iocpdesc(s);
  iocpd->iocp = iocp;
  if (iocpd) {
    iocpd->external = external;
    iocpd->error = pn_error();
    if (listening) {
      iocpd->acceptor = pni_acceptor(iocpd);
    } else {
      iocpd->pipeline = pni_write_pipeline(iocpd);
      iocpd->read_result = read_result(iocpd);
    }
    pni_iocpdesc_map_push(iocpd);
  }
  return iocpd;
}

// === Fast lookup of a socket's iocpdesc_t

iocpdesc_t *pni_iocpdesc_map_get(iocp_t *iocp, pn_socket_t s) {
  iocpdesc_t *iocpd = (iocpdesc_t *) pn_hash_get(iocp->iocpdesc_map, s);
  return iocpd;
}

void pni_iocpdesc_map_push(iocpdesc_t *iocpd) {
  pn_hash_put(iocpd->iocp->iocpdesc_map, iocpd->socket, iocpd);
  pn_decref(iocpd);
  assert(pn_refcount(iocpd) == 1);
}

void pni_iocpdesc_map_del(iocp_t *iocp, pn_socket_t s) {
  pn_hash_del(iocp->iocpdesc_map, (uintptr_t) s);
}

static void bind_to_completion_port(iocpdesc_t *iocpd)
{
  if (iocpd->bound) return;
  if (!iocpd->iocp->completion_port) {
    iocpdesc_fail(iocpd, WSAEINVAL, "Incomplete setup, no completion port.");
    return;
  }

  if (CreateIoCompletionPort ((HANDLE) iocpd->socket, iocpd->iocp->completion_port, 0, 0))
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
    complete(result, good_op, num_transferred);
  }
}

// returns: -1 on error, 0 on timeout, 1 successful completion
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
  iocpd->reap_time = pn_i_now() + 2000;
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
    assert(iocpd->ops_in_progress > 0);
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

  pn_timestamp_t now = pn_i_now();
  pn_timestamp_t deadline = now + 2000;

  while (pn_list_size(iocp->zombie_list)) {
    if (now >= deadline)
      break;
    int rv = pni_iocp_wait_one(iocp, deadline - now, NULL);
    if (rv < 0) {
      iocp_log("unexpected IOCP failure on Proton IO shutdown %d\n", GetLastError());
      break;
    }
    now = pn_i_now();
  }
  if (now >= deadline && pn_list_size(iocp->zombie_list))
    // Should only happen if really slow TCP handshakes, i.e. total network failure
    iocp_log("network failure on Proton shutdown\n");
}

static pn_list_t *iocp_map_close_all(iocp_t *iocp)
{
  // Zombify stragglers, i.e. no pn_close() from the application.
  pn_list_t *externals = pn_list(PN_OBJECT, 0);
  for (pn_handle_t entry = pn_hash_head(iocp->iocpdesc_map); entry;
       entry = pn_hash_next(iocp->iocpdesc_map, entry)) {
    iocpdesc_t *iocpd = (iocpdesc_t *) pn_hash_value(iocp->iocpdesc_map, entry);
    // Just listeners first.
    if (is_listener(iocpd)) {
      if (iocpd->external) {
        // Owned by application, just keep a temporary reference to it.
        // iocp_result_t structs must not be free'd until completed or
        // the completion port is closed.
        if (iocpd->ops_in_progress)
          pn_list_add(externals, iocpd);
        pni_iocpdesc_map_del(iocp, iocpd->socket);
      } else {
        // Make it a zombie.
        pni_iocp_begin_close(iocpd);
      }
    }
  }
  pni_iocp_drain_completions(iocp);

  for (pn_handle_t entry = pn_hash_head(iocp->iocpdesc_map); entry;
       entry = pn_hash_next(iocp->iocpdesc_map, entry)) {
    iocpdesc_t *iocpd = (iocpdesc_t *) pn_hash_value(iocp->iocpdesc_map, entry);
    if (iocpd->external) {
      iocpd->read_closed = true;   // Do not consume from read side
      iocpd->write_closed = true;  // Do not shutdown write side
      if (iocpd->ops_in_progress)
        pn_list_add(externals, iocpd);
      pni_iocpdesc_map_del(iocp, iocpd->socket);
    } else {
      // Make it a zombie.
      pni_iocp_begin_close(iocpd);
    }
  }
  return externals;
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
  if (shutdown(iocpd->socket, SD_SEND)) {
    if (iocpd->iocp->iocp_trace)
      iocp_log("socket shutdown failed %d\n", WSAGetLastError());
  }
  iocpd->write_closed = true;
  if (iocpd->read_closed) {
    closesocket(iocpd->socket);
    iocpd->socket = INVALID_SOCKET;
  }
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
    pni_iocpdesc_map_del(iocpd->iocp, old_sock);  // may pn_free *iocpd
  } else {
    // Continue async operation looking for graceful close confirmation or timeout.
    pn_socket_t old_sock = iocpd->socket;
    iocpd->closing = true;
    if (!iocpd->write_closed && !write_in_progress(iocpd))
      iocp_shutdown(iocpd);
    zombie_list_add(iocpd);
    pni_iocpdesc_map_del(iocpd->iocp, old_sock);  // may pn_free *iocpd
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
  iocp->iocpdesc_map = pn_hash(PN_OBJECT, 0, 0.75);
  iocp->zombie_list = pn_list(PN_OBJECT, 0);
  iocp->iocp_trace = pn_env_bool("PN_TRACE_DRV");
  iocp->selector = NULL;
}

void pni_iocp_finalize(void *obj)
{
  iocp_t *iocp = (iocp_t *) obj;
  // Move sockets to closed state, except external sockets.
  pn_list_t *externals = iocp_map_close_all(iocp);
  // Now everything with ops_in_progress is in the zombie_list or the externals list.
  assert(!pn_hash_head(iocp->iocpdesc_map));
  pn_free(iocp->iocpdesc_map);

  drain_zombie_completions(iocp);    // Last chance for graceful close
  zombie_list_hard_close_all(iocp);
  CloseHandle(iocp->completion_port);  // This cancels all our async ops
  iocp->completion_port = NULL;

  if (pn_list_size(externals) && iocp->iocp_trace)
    iocp_log("%d external sockets not closed and removed from Proton IOCP control\n", pn_list_size(externals));

  // Now safe to free everything that might be touched by a former async operation.
  pn_free(externals);
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
