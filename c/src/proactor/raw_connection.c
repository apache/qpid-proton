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

/* Enable POSIX features beyond c99 for modern pthread and standard strerror_r() */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
/* Avoid GNU extensions, in particular the incompatible alternative strerror_r() */
#undef _GNU_SOURCE

#include "proton/raw_connection.h"

#include "proton/event.h"
#include "proton/listener.h"
#include "proton/object.h"
#include "proton/proactor.h"
#include "proton/types.h"

#include "core/util.h"
#include "proactor-internal.h"

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "raw_connection-internal.h"

PN_STRUCT_CLASSDEF(pn_raw_connection)

void pni_raw_initialize(pn_raw_connection_t *conn) {
  // Link together free lists
  for (buff_ptr i = 1; i<=read_buffer_count; i++) {
    conn->rbuffers[i-1].next = i==read_buffer_count ? 0 : i+1;
    conn->rbuffers[i-1].type = buff_rempty;
    conn->wbuffers[i-1].next = i==read_buffer_count ? 0 : i+1;
    conn->wbuffers[i-1].type = buff_wempty;
  }

  conn->condition = pn_condition();
  conn->collector = pn_collector();
  conn->attachments = pn_record();

  conn->rbuffer_first_empty = 1;
  conn->wbuffer_first_empty = 1;

  conn->state = conn_init;
}

typedef enum {
  conn_connected     = 0,
  conn_read_closed   = 1,
  conn_write_closed  = 2,
  conn_write_drained = 3,
  int_read_shutdown  = 4,
  int_write_shutdown = 5,
  int_disconnect     = 6,
  api_read_close     = 7,
  api_write_close    = 8,
} raw_event;

/*
 * There's a little trick in this state table - both conn_init - the initial state and the self transition are represented by
 * 0. This is because no state ever transitions to the initial state.
 */
const uint8_t state_transitions[][9] = {
  /* State\event  cconnected, crclosed,   cwclosed,   cwdrained,  irshutdown, iwshutdown, idsconnect, arclose,    awclose   */
  [conn_init]  = {conn_ro_wo, conn_wrong, conn_wrong, conn_wrong, conn_wrong, conn_wrong, conn_wrong, conn_rc_wo, conn_ro_wd},
  [conn_ro_wo] = {conn_wrong, conn_rc_wo, conn_ro_wc, 0,          conn_wrong, conn_wrong, conn_wrong, conn_rc_wo, conn_ro_wd},
  [conn_ro_wd] = {conn_wrong, conn_rc_wd, conn_ro_wc, conn_ro_wc, conn_wrong, conn_wrong, conn_wrong, conn_rc_wd, 0},
  [conn_ro_wc] = {conn_wrong, conn_rs_ws, 0,          conn_wrong, conn_wrong, conn_ro_ws, conn_wrong, conn_rs_ws, 0},
  [conn_ro_ws] = {conn_wrong, conn_rs_ws, 0,          0,          conn_wrong, conn_wrong, conn_wrong, conn_rs_ws, 0},
  [conn_rc_wo] = {conn_wrong, 0,          conn_rs_ws, 0,          conn_rs_wo, conn_wrong, conn_wrong, 0,          conn_rc_wd},
  [conn_rc_wd] = {conn_wrong, 0,          conn_rs_ws, conn_rs_ws, conn_rs_wd, conn_wrong, conn_wrong, 0,          0},
  [conn_rs_wo] = {conn_wrong, 0,          conn_rs_ws, 0,          conn_wrong, conn_wrong, conn_wrong, 0,          conn_rs_wd},
  [conn_rs_wd] = {conn_wrong, 0,          conn_rs_ws, conn_rs_ws, conn_wrong, conn_wrong, conn_wrong, 0,          0},
  [conn_rs_ws] = {conn_wrong, 0,          0,          conn_wrong, conn_wrong, conn_wrong, conn_fini,  0,          0},
  [conn_fini]  = {conn_wrong, conn_wrong, conn_wrong, conn_wrong, conn_wrong, conn_wrong, 0,          0,          0},
};

static inline uint8_t pni_raw_new_state(pn_raw_connection_t *conn, raw_event event) {
  uint8_t old_state = conn->state;
  uint8_t new_state = state_transitions[old_state][event];
  assert(new_state != (uint8_t)conn_wrong);
  return new_state == 0 ? old_state : new_state;
}

// Open for read by application
static inline bool pni_raw_ropen(pn_raw_connection_t *conn) {
  switch (conn->state) {
    case conn_ro_wo:
    case conn_ro_wd:
    case conn_ro_wc:
    case conn_ro_ws:
      return true;
    default:
      return false;
  }
}

// Closed for reading by application
static inline bool pni_raw_rclosed(pn_raw_connection_t *conn) {
  switch (conn->state) {
    case conn_rc_wo:
    case conn_rc_wd:
    case conn_rs_wo:
    case conn_rs_wd:
    case conn_rs_ws:
    case conn_fini :
      return true;
    default:
      return false;
  }
}

// Not yet sent read closed event
static inline bool pni_raw_rclosing(pn_raw_connection_t *conn) {
  switch (conn->state) {
    case conn_rc_wo:
    case conn_rc_wd:
      return true;
    default:
      return false;
  }
}

// Unused code
#if 0
// Open for write by application
static inline bool pni_raw_wopen(pn_raw_connection_t *conn) {
  switch (conn->state) {
    case conn_ro_wo:
    case conn_rc_wo:
    case conn_rs_wo:
      return true;
    default:
      return false;
  }
}
#endif

// Return whether closed for writing by application (name a little confusing!)
// NB Initial state is NOT closed for writing - app can write before connecting
static inline bool pni_raw_wclosed(pn_raw_connection_t *conn) {
  switch (conn->state) {
    case conn_ro_wd:
    case conn_ro_wc:
    case conn_ro_ws:
    case conn_rc_wd:
    case conn_rs_wd:
    case conn_rs_ws:
    case conn_fini :
      return true;
    default:
      return false;
  }
}

// Return whether closed for application and all writes drained
static inline bool pni_raw_wdrained(pn_raw_connection_t *conn) {
  switch (conn->state) {
    case conn_ro_wc:
    case conn_ro_ws:
    case conn_rs_ws:
    case conn_fini :
      return true;
    default:
      return false;
  }
}

// Not yet sent write closed event
static inline bool pni_raw_wclosing(pn_raw_connection_t *conn) {
  switch (conn->state) {
    case conn_ro_wc:
      return true;
    default:
      return false;
  }
}

// Fully closed for read/write - sent both closed events 
static inline bool pni_raw_rwclosed(pn_raw_connection_t *conn) {
  switch (conn->state) {
    case conn_rs_ws:
    case conn_fini :
      return true;
    default:
      return false;
  }
}

bool pni_raw_validate(pn_raw_connection_t *conn) {
  int rempty_count = 0;
  for (buff_ptr i = conn->rbuffer_first_empty; i; i = conn->rbuffers[i-1].next) {
    if (conn->rbuffers[i-1].type != buff_rempty) return false;
    rempty_count++;
  }
  int runused_count = 0;
  for (buff_ptr i = conn->rbuffer_first_unused; i; i = conn->rbuffers[i-1].next) {
    if (conn->rbuffers[i-1].type != buff_unread) return false;
    runused_count++;
  }
  int rread_count = 0;
  for (buff_ptr i = conn->rbuffer_first_read; i; i = conn->rbuffers[i-1].next) {
    if (conn->rbuffers[i-1].type != buff_read) return false;
    rread_count++;
  }
  if (rempty_count+runused_count+rread_count != read_buffer_count) return false;
  if (!conn->rbuffer_first_unused && conn->rbuffer_last_unused) return false;
  if (conn->rbuffer_last_unused &&
    (conn->rbuffers[conn->rbuffer_last_unused-1].type != buff_unread || conn->rbuffers[conn->rbuffer_last_unused-1].next != 0)) return false;
  if (!conn->rbuffer_first_read && conn->rbuffer_last_read) return false;
  if (conn->rbuffer_last_read &&
    (conn->rbuffers[conn->rbuffer_last_read-1].type != buff_read || conn->rbuffers[conn->rbuffer_last_read-1].next != 0)) return false;

  int wempty_count = 0;
  for (buff_ptr i = conn->wbuffer_first_empty; i; i = conn->wbuffers[i-1].next) {
    if (conn->wbuffers[i-1].type != buff_wempty) return false;
    wempty_count++;
  }
  int wunwritten_count = 0;
  for (buff_ptr i = conn->wbuffer_first_towrite; i; i = conn->wbuffers[i-1].next) {
    if (conn->wbuffers[i-1].type != buff_unwritten) return false;
    wunwritten_count++;
  }
  int wwritten_count = 0;
  for (buff_ptr i = conn->wbuffer_first_written; i; i = conn->wbuffers[i-1].next) {
    if (conn->wbuffers[i-1].type != buff_written) return false;
    wwritten_count++;
  }
  if (wempty_count+wunwritten_count+wwritten_count != write_buffer_count) return false;
  if (!conn->wbuffer_first_towrite && conn->wbuffer_last_towrite) return false;
  if (conn->wbuffer_last_towrite &&
    (conn->wbuffers[conn->wbuffer_last_towrite-1].type != buff_unwritten || conn->wbuffers[conn->wbuffer_last_towrite-1].next != 0)) return false;
  if (!conn->wbuffer_first_written && conn->wbuffer_last_written) return false;
  if (conn->wbuffer_last_written &&
    (conn->wbuffers[conn->wbuffer_last_written-1].type != buff_written || conn->wbuffers[conn->wbuffer_last_written-1].next != 0)) return false;
  return true;
}

void pni_raw_finalize(pn_raw_connection_t *conn) {
  pn_condition_free(conn->condition);
  pn_collector_free(conn->collector);
  pn_free(conn->attachments);
}

size_t pn_raw_connection_read_buffers_capacity(pn_raw_connection_t *conn) {
  assert(conn);
  bool rclosed = pni_raw_rclosed(conn);
  return rclosed ? 0 : (read_buffer_count - conn->rbuffer_count);
}

size_t pn_raw_connection_write_buffers_capacity(pn_raw_connection_t *conn) {
  assert(conn);
  bool wclosed = pni_raw_wclosed(conn);
  return wclosed ? 0 : (write_buffer_count-conn->wbuffer_count);
}

size_t pn_raw_connection_give_read_buffers(pn_raw_connection_t *conn, pn_raw_buffer_t const *buffers, size_t num) {
  assert(conn);
  size_t can_take = pn_min(num, pn_raw_connection_read_buffers_capacity(conn));
  if ( can_take==0 ) return 0;

  buff_ptr current = conn->rbuffer_first_empty;
  assert(current);

  buff_ptr previous;
  for (size_t i = 0; i < can_take; i++) {
    // Get next free
    assert(conn->rbuffers[current-1].type == buff_rempty);
    conn->rbuffers[current-1].context = buffers[i].context;
    conn->rbuffers[current-1].bytes = buffers[i].bytes;
    conn->rbuffers[current-1].capacity = buffers[i].capacity;
    conn->rbuffers[current-1].size = 0;
    conn->rbuffers[current-1].offset = buffers[i].offset;
    conn->rbuffers[current-1].type = buff_unread;

    previous = current;
    current = conn->rbuffers[current-1].next;
  }
  if (!conn->rbuffer_last_unused) {
    conn->rbuffer_last_unused = previous;
  }

  conn->rbuffers[previous-1].next = conn->rbuffer_first_unused;
  conn->rbuffer_first_unused = conn->rbuffer_first_empty;
  conn->rbuffer_first_empty = current;

  conn->rbuffer_count += can_take;
  conn->rrequestedbuffers = false;
  return can_take;
}

size_t pn_raw_connection_take_read_buffers(pn_raw_connection_t *conn, pn_raw_buffer_t *buffers, size_t num) {
  assert(conn);
  size_t count = 0;

  buff_ptr current = conn->rbuffer_first_read;
  if (!current) return 0;

  buff_ptr previous;
  for (; current && count < num; count++) {
    assert(conn->rbuffers[current-1].type == buff_read);
    buffers[count].context = conn->rbuffers[current-1].context;
    buffers[count].bytes = conn->rbuffers[current-1].bytes;
    buffers[count].capacity = conn->rbuffers[current-1].capacity;
    buffers[count].size = conn->rbuffers[current-1].size;
    buffers[count].offset = conn->rbuffers[current-1].offset - conn->rbuffers[current-1].size;
    conn->rbuffers[current-1].type = buff_rempty;

    previous = current;
    current = conn->rbuffers[current-1].next;
  }
  if (!count) return 0;

  conn->rbuffers[previous-1].next = conn->rbuffer_first_empty;
  conn->rbuffer_first_empty = conn->rbuffer_first_read;

  conn->rbuffer_first_read = current;
  if (!current) {
    conn->rbuffer_last_read = 0;
  }
  conn->rbuffer_count -= count;
  return count;
}

size_t pn_raw_connection_write_buffers(pn_raw_connection_t *conn, pn_raw_buffer_t const *buffers, size_t num) {
  assert(conn);
  size_t can_take = pn_min(num, pn_raw_connection_write_buffers_capacity(conn));
  if ( can_take==0 ) return 0;

  buff_ptr current = conn->wbuffer_first_empty;
  assert(current);

  buff_ptr previous;
  for (size_t i = 0; i < can_take; i++) {
    // Get next free
    assert(conn->wbuffers[current-1].type == buff_wempty);
    conn->wbuffers[current-1].context = buffers[i].context;
    conn->wbuffers[current-1].bytes = buffers[i].bytes;
    conn->wbuffers[current-1].capacity = buffers[i].capacity;
    conn->wbuffers[current-1].size = buffers[i].size;
    conn->wbuffers[current-1].offset = buffers[i].offset;
    conn->wbuffers[current-1].type = buff_unwritten;

    previous = current;
    current = conn->wbuffers[current-1].next;
  }

  if (!conn->wbuffer_first_towrite) {
    conn->wbuffer_first_towrite = conn->wbuffer_first_empty;
  }
  if (conn->wbuffer_last_towrite) {
    conn->wbuffers[conn->wbuffer_last_towrite-1].next = conn->wbuffer_first_empty;
  }

  conn->wbuffer_last_towrite = previous;
  conn->wbuffers[previous-1].next = 0;
  conn->wbuffer_first_empty = current;

  conn->wbuffer_count += can_take;
  conn->wrequestedbuffers = false;
  return can_take;
}

size_t pn_raw_connection_take_written_buffers(pn_raw_connection_t *conn, pn_raw_buffer_t *buffers, size_t num) {
  assert(conn);
  size_t count = 0;

  buff_ptr current = conn->wbuffer_first_written;
  if (!current) return 0;

  buff_ptr previous;
  for (; current && count < num; count++) {
    assert(conn->wbuffers[current-1].type == buff_written);
    buffers[count].context = conn->wbuffers[current-1].context;
    buffers[count].bytes = conn->wbuffers[current-1].bytes;
    buffers[count].capacity = conn->wbuffers[current-1].capacity;
    buffers[count].size = conn->wbuffers[current-1].size;
    buffers[count].offset = conn->wbuffers[current-1].offset;
    conn->wbuffers[current-1].type = buff_wempty;

    previous = current;
    current = conn->wbuffers[current-1].next;
  }
  if (!count) return 0;

  conn->wbuffers[previous-1].next = conn->wbuffer_first_empty;
  conn->wbuffer_first_empty = conn->wbuffer_first_written;

  conn->wbuffer_first_written = current;
  if (!current) {
    conn->wbuffer_last_written = 0;
  }
  conn->wbuffer_count -= count;
  return count;
}

static inline void pni_raw_put_event(pn_raw_connection_t *conn, pn_event_type_t type) {
  pn_collector_put(conn->collector, PN_CLASSCLASS(pn_raw_connection), (void*)conn, type);
}

static inline void pni_raw_release_buffers(pn_raw_connection_t *conn) {
  for(;conn->rbuffer_first_unused;) {
    buff_ptr p = conn->rbuffer_first_unused;
    assert(conn->rbuffers[p-1].type == buff_unread);
    conn->rbuffers[p-1].size = 0;
    if (!conn->rbuffer_first_read) {
      conn->rbuffer_first_read = p;
    }
    if (conn->rbuffer_last_read) {
      conn->rbuffers[conn->rbuffer_last_read-1].next = p;
    }
    conn->rbuffer_last_read = p;
    conn->rbuffer_first_unused = conn->rbuffers[p-1].next;

    conn->rbuffers[p-1].next = 0;
    conn->rbuffers[p-1].type = buff_read;
  }
  conn->rbuffer_last_unused = 0;
  for(;conn->wbuffer_first_towrite;) {
    buff_ptr p = conn->wbuffer_first_towrite;
    assert(conn->wbuffers[p-1].type == buff_unwritten);
    if (!conn->wbuffer_first_written) {
      conn->wbuffer_first_written = p;
    }
    if (conn->wbuffer_last_written) {
      conn->wbuffers[conn->wbuffer_last_written-1].next = p;
    }
    conn->wbuffer_last_written = p;
    conn->wbuffer_first_towrite = conn->wbuffers[p-1].next;

    conn->wbuffers[p-1].next = 0;
    conn->wbuffers[p-1].type = buff_written;
  }
  conn->wbuffer_last_towrite = 0;
}

static inline void pni_raw_disconnect(pn_raw_connection_t *conn) {
  pni_raw_release_buffers(conn);
  conn->disconnectpending = true;
  conn->disconnect_state  = disc_init;
  conn->state = pni_raw_new_state(conn, int_disconnect);
}

void pni_raw_connected(pn_raw_connection_t *conn) {
  pn_condition_clear(conn->condition);
  pni_raw_put_event(conn, PN_RAW_CONNECTION_CONNECTED);
  conn->state = pni_raw_new_state(conn, conn_connected);
}

void pni_raw_connect_failed(pn_raw_connection_t *conn) {
  conn->state = conn_fini;
  pni_raw_disconnect(conn);
}

void pni_raw_wake(pn_raw_connection_t *conn) {
  conn->wakepending = true;
}

void pni_raw_read(pn_raw_connection_t *conn, int sock, long (*recv)(int, void*, size_t), void(*set_error)(pn_raw_connection_t *, const char *, int)) {
  assert(conn);

  if (!pni_raw_ropen(conn)) return;

  bool closed = false;
  for(;conn->rbuffer_first_unused;) {
    buff_ptr p = conn->rbuffer_first_unused;
    assert(conn->rbuffers[p-1].type == buff_unread);
    char *bytes = conn->rbuffers[p-1].bytes+conn->rbuffers[p-1].offset;
    size_t s = conn->rbuffers[p-1].capacity-conn->rbuffers[p-1].offset;
    int r = recv(sock, bytes, s);
    if (r < 0) {
      switch (errno) {
        // Interrupted system call try again
        case EINTR: continue;

        // Would block
        case EWOULDBLOCK: goto finished_reading;

        // Detected an error
        default:
          set_error(conn, "recv error", errno);
          pni_raw_close(conn);
          return;
      }
    }
    conn->rbuffers[p-1].size += r;
    conn->rbuffers[p-1].offset += r;

    if (!conn->rbuffer_first_read) {
      conn->rbuffer_first_read = p;
    }
    if (conn->rbuffer_last_read) {
      conn->rbuffers[conn->rbuffer_last_read-1].next = p;
    }
    conn->rbuffer_last_read = p;
    conn->rbuffer_first_unused = conn->rbuffers[p-1].next;

    conn->rbuffers[p-1].next = 0;
    conn->rbuffers[p-1].type = buff_read;

    // Checking for end of stream here ensures that there is a buffer at the end with nothing in it
    if (r == 0) {
      closed = true;
      break;
    }
  }
finished_reading:
  if (!conn->rbuffer_first_unused) {
    conn->rbuffer_last_unused = 0;
  }
  // Read something - we are now either out of buffers; end of stream; or blocked for read
  if (conn->rbuffer_first_read && !conn->rpending) {
    conn->rpending = true;
  }

  uint8_t old_state = conn->state;

  // Socket closed for read
  if (closed) {
    conn->state = pni_raw_new_state(conn, conn_read_closed);
    conn->rclosedpending = true;
  }

  if (conn->state != old_state) {
    if (pni_raw_rwclosed(conn)) {
      pni_raw_disconnect(conn);
    }
  }
  return;
}

void pni_raw_write(pn_raw_connection_t *conn, int sock, long (*send)(int, const void*, size_t), void(*set_error)(pn_raw_connection_t *, const char *, int)) {
  assert(conn);

  if (pni_raw_wdrained(conn)) return;

  bool closed = false;
  bool drained = false;
  for(;conn->wbuffer_first_towrite;) {
    buff_ptr p = conn->wbuffer_first_towrite;
    assert(conn->wbuffers[p-1].type == buff_unwritten);
    char *bytes = conn->wbuffers[p-1].bytes+conn->wbuffers[p-1].offset+conn->unwritten_offset;
    size_t s = conn->wbuffers[p-1].size-conn->unwritten_offset;
    int r = send(sock,  bytes, s);
    if (r < 0) {
      // Interrupted system call try again
      switch (errno) {
        // Interrupted system call try again
        case EINTR: continue;

        case EWOULDBLOCK:
          goto finished_writing;

        default:
          set_error(conn, "send error", errno);
          pni_raw_release_buffers(conn);
          pni_raw_close(conn);
          return;
      }
    }
    // return of 0 was never observed in testing and the documentation
    // implies that 0 could only be returned if 0 bytes were sent; however
    // leaving this case here seems safe.
    if (r == 0 && s > 0) {
      closed = true;
      break;
    }

    // Only wrote a partial buffer  - adjust buffer
    if (r != (int)s) {
      conn->unwritten_offset += r;
      break;
    }

    conn->unwritten_offset = 0;

    if (!conn->wbuffer_first_written) {
      conn->wbuffer_first_written = p;
    }
    if (conn->wbuffer_last_written) {
      conn->wbuffers[conn->wbuffer_last_written-1].next = p;
    }
    conn->wbuffer_last_written = p;
    conn->wbuffer_first_towrite = conn->wbuffers[p-1].next;

    conn->wbuffers[p-1].next = 0;
    conn->wbuffers[p-1].type = buff_written;
  }
finished_writing:
  if (!conn->wbuffer_first_towrite) {
    conn->wbuffer_last_towrite = 0;
    drained = true;
  }
  // Wrote something; end of stream; out of buffers; or blocked for write
  if (conn->wbuffer_first_written && !conn->wpending) {
    conn->wpending = true;
  }

  uint8_t old_state = conn->state;

  // Drained all writes
  if (drained) {
    conn->state = pni_raw_new_state(conn, conn_write_drained);
  }

  // Socket closed for write
  if (closed) {
    conn->state = pni_raw_new_state(conn, conn_write_closed);
    conn->wclosedpending = true;
  }

  if (conn->state != old_state) {
    if (pni_raw_rwclosed(conn)) {
      pni_raw_disconnect(conn);
    }
  }
  return;
}

void pni_raw_process_shutdown(pn_raw_connection_t *conn, int sock, int (*shutdown_rd)(int), int (*shutdown_wr)(int)) {
  assert(conn);
  if (pni_raw_rclosing(conn)) {
    shutdown_rd(sock);
    conn->state = pni_raw_new_state(conn, int_read_shutdown);
  }
  if (pni_raw_wclosing(conn)) {
    shutdown_wr(sock);
    conn->state = pni_raw_new_state(conn, int_write_shutdown);
  }
}

bool pni_raw_can_read(pn_raw_connection_t *conn) {
  return pni_raw_ropen(conn) && conn->rbuffer_first_unused;
}

bool pni_raw_can_write(pn_raw_connection_t *conn) {
  return !pni_raw_wdrained(conn) && conn->wbuffer_first_towrite;
}

pn_event_t *pni_raw_event_next(pn_raw_connection_t *conn) {
  assert(conn);
  do {
    pn_event_t *event = pn_collector_next(conn->collector);
    if (event) {
      return pni_log_event(conn, event);
    } else if (conn->wakepending) {
      pni_raw_put_event(conn, PN_RAW_CONNECTION_WAKE);
      conn->wakepending = false;
    } else if (conn->rpending) {
      pni_raw_put_event(conn, PN_RAW_CONNECTION_READ);
      conn->rpending = false;
    } else if (conn->wpending) {
      pni_raw_put_event(conn, PN_RAW_CONNECTION_WRITTEN);
      conn->wpending = false;
    } else if (conn->rclosedpending) {
      pni_raw_put_event(conn, PN_RAW_CONNECTION_CLOSED_READ);
      conn->rclosedpending = false;
    } else if (conn->wclosedpending) {
      pni_raw_put_event(conn, PN_RAW_CONNECTION_CLOSED_WRITE);
      conn->wclosedpending = false;
    } else if (conn->disconnectpending) {
      switch (conn->disconnect_state) {
      case disc_init:
        if (conn->rbuffer_first_read || conn->wbuffer_first_written) {
          pni_raw_put_event(conn, PN_RAW_CONNECTION_DRAIN_BUFFERS);
        }
        conn->disconnect_state = disc_drain_msg;
        break;
      // TODO: We'll leave the read/written events in here for the moment for backward compatibility
      // remove them soon (after dispatch uses DRAIN_BUFFER)
      case disc_drain_msg:
        if (conn->rbuffer_first_read) {
          pni_raw_put_event(conn, PN_RAW_CONNECTION_READ);
        }
        conn->disconnect_state = disc_read_msg;
        break;
      case disc_read_msg:
        if (conn->wbuffer_first_written) {
          pni_raw_put_event(conn, PN_RAW_CONNECTION_WRITTEN);
        }
        conn->disconnect_state = disc_written_msg;
        break;
      case disc_written_msg:
        pni_raw_put_event(conn, PN_RAW_CONNECTION_DISCONNECTED);
        conn->disconnectpending = false;
        conn->disconnect_state = disc_fini;
        break;
      }
    } else if (!pni_raw_wdrained(conn) && !conn->wbuffer_first_towrite && !conn->wrequestedbuffers) {
      // Ran out of write buffers
      pni_raw_put_event(conn, PN_RAW_CONNECTION_NEED_WRITE_BUFFERS);
      conn->wrequestedbuffers = true;
    } else if (!pni_raw_rclosed(conn) && !conn->rbuffer_first_unused && !conn->rrequestedbuffers) {
      // Ran out of read buffers
      pni_raw_put_event(conn, PN_RAW_CONNECTION_NEED_READ_BUFFERS);
      conn->rrequestedbuffers = true;
    } else {
      return NULL;
    }
  } while (true);
}

void pni_raw_read_close(pn_raw_connection_t *conn) {
  // If already fully closed nothing to do
  if (pni_raw_rwclosed(conn)) {
    return;
  }
  if (!pni_raw_rclosed(conn)) {
    conn->rclosedpending = true;
  }
  conn->state = pni_raw_new_state(conn, api_read_close);
  if (pni_raw_rwclosed(conn)) {
    pni_raw_disconnect(conn);
  }
}

void pni_raw_write_close(pn_raw_connection_t *conn) {
  // If already fully closed nothing to do
  if (pni_raw_rwclosed(conn)) {
    return;
  }

  if (!pni_raw_wdrained(conn)) {
    if (!pni_raw_wclosed(conn)) {
      conn->wclosedpending = true;
    }
    conn->state = pni_raw_new_state(conn, api_write_close);

    if (!conn->wbuffer_first_towrite) {
      conn->state = pni_raw_new_state(conn, conn_write_drained);
    }
  }
  if (pni_raw_rwclosed(conn)) {
    pni_raw_disconnect(conn);
  }
}

void pni_raw_close(pn_raw_connection_t *conn) {
  // If already fully closed nothing to do
  if (pni_raw_rwclosed(conn)) {
    return;
  }
  if (!pni_raw_rclosed(conn)) {
    conn->rclosedpending = true;
  }
  conn->state = pni_raw_new_state(conn, api_read_close);

  if (!pni_raw_wdrained(conn)) {
    if (!pni_raw_wclosed(conn)) {
      conn->wclosedpending = true;
    }
    conn->state = pni_raw_new_state(conn, api_write_close);

    if (!conn->wbuffer_first_towrite) {
      conn->state = pni_raw_new_state(conn, conn_write_drained);
    }
  }
  if (pni_raw_rwclosed(conn)) {
    pni_raw_disconnect(conn);
  }
}

bool pn_raw_connection_is_read_closed(pn_raw_connection_t *conn) {
  assert(conn);
  return pni_raw_rclosed(conn);
}

bool pn_raw_connection_is_write_closed(pn_raw_connection_t *conn) {
  assert(conn);
  return pni_raw_wclosed(conn);
}

pn_condition_t *pn_raw_connection_condition(pn_raw_connection_t *conn) {
  assert(conn);
  return conn->condition;
}

void *pn_raw_connection_get_context(pn_raw_connection_t *conn) {
  assert(conn);
  return pn_record_get(conn->attachments, PN_LEGCTX);
}

void pn_raw_connection_set_context(pn_raw_connection_t *conn, void *context) {
  assert(conn);
  pn_record_set(conn->attachments, PN_LEGCTX, context);
}

pn_record_t *pn_raw_connection_attachments(pn_raw_connection_t *conn) {
  assert(conn);
  return conn->attachments;
}

pn_raw_connection_t *pn_event_raw_connection(pn_event_t *event) {
  return (pn_event_class(event) == PN_CLASSCLASS(pn_raw_connection)) ? (pn_raw_connection_t*)pn_event_context(event) : NULL;
}
