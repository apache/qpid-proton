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

/*
 * A simple write buffer pool.  Each socket has a dedicated "primary"
 * buffer and can borrow from a shared pool with limited size tuning.
 * Could enhance e.g. with separate pools per network interface and fancier
 * memory tuning based on interface speed, system resources, and
 * number of connections, etc.
 */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#if _WIN32_WINNT < 0x0501
#error "Proton requires Windows API support for XP or later."
#endif
#include <winsock2.h>
#include <Ws2tcpip.h>
#define PN_WINAPI

#include "platform.h"
#include <proton/object.h>
#include <proton/io.h>
#include <proton/selector.h>
#include <proton/error.h>
#include <assert.h>
#include "selectable.h"
#include "util.h"
#include "iocp.h"

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
    for (int i = 0; i < iocp->shared_pool_size; i++) {
      iocp->shared_results[i] = iocp->available_results[i] = pni_write_result(NULL, mem, IOCP_WBUFSIZE);
      mem += IOCP_WBUFSIZE;
    }
  }
}

void pni_shared_pool_free(iocp_t *iocp)
{
  for (int i = 0; i < iocp->shared_pool_size; i++) {
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
  size_t bufs = pn_min(wanted, pl->depth - pending);
  // Can draw from shared pool or the primary... but share with others.
  size_t writers = iocp->writer_count;
  size_t shared_count = (iocp->shared_available_count + writers - 1) / writers;
  bufs = pn_min(bufs, shared_count + 1);
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
