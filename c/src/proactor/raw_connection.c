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
  return read_buffer_count - conn->rbuffer_count;
}

size_t pn_raw_connection_write_buffers_capacity(pn_raw_connection_t *conn) {
  assert(conn);
  return write_buffer_count-conn->wbuffer_count;
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
  conn->rneedbufferevent = false;
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
  conn->wneedbufferevent = false;
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
  conn->rdrainpending = (bool)(conn->rbuffer_first_read);
  conn->wdrainpending = (bool)(conn->wbuffer_first_written);
}

static inline void pni_raw_disconnect(pn_raw_connection_t *conn) {
  pni_raw_release_buffers(conn);
  conn->disconnectpending = true;
}

void pni_raw_connected(pn_raw_connection_t *conn) {
  pn_condition_clear(conn->condition);
  pni_raw_put_event(conn, PN_RAW_CONNECTION_CONNECTED);
}

void pni_raw_connect_failed(pn_raw_connection_t *conn) {
  conn->rclosed = true;
  conn->wclosed = true;
  pni_raw_disconnect(conn);
}

void pni_raw_wake(pn_raw_connection_t *conn) {
  conn->wakepending = true;
}

void pni_raw_read(pn_raw_connection_t *conn, int sock, long (*recv)(int, void*, size_t), void(*set_error)(pn_raw_connection_t *, const char *, int)) {
  assert(conn);
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
  // Socket closed for read
  if (closed) {
    if (!conn->rclosed) {
      conn->rclosed = true;
      conn->rclosedpending = true;
      if (conn->wclosed) {
        pni_raw_disconnect(conn);
      }
    }
  }
  return;
}

void pni_raw_write(pn_raw_connection_t *conn, int sock, long (*send)(int, const void*, size_t), void(*set_error)(pn_raw_connection_t *, const char *, int)) {
  assert(conn);
  bool closed = false;
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
    closed = closed || conn->wdraining;
  }
  // Wrote something; end of stream; out of buffers; or blocked for write
  if (conn->wbuffer_first_written && !conn->wpending) {
    conn->wpending = true;
  }
  // Socket closed for write
  if (closed) {
    if (!conn->wclosed) {
      conn->wclosed = true;
      conn->wclosedpending = true;
      if (conn->rclosed) {
        pni_raw_disconnect(conn);;
      }
    }
  }
  return;
}

bool pni_raw_can_read(pn_raw_connection_t *conn) {
  return !conn->rclosed && conn->rbuffer_first_unused;
}

bool pni_raw_can_write(pn_raw_connection_t *conn) {
  return !conn->wclosed && conn->wbuffer_first_towrite;
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
    } else if (conn->rdrainpending) {
      pni_raw_put_event(conn, PN_RAW_CONNECTION_READ);
      conn->rdrainpending = false;
    } else if (conn->wdrainpending) {
      pni_raw_put_event(conn, PN_RAW_CONNECTION_WRITTEN);
      conn->wdrainpending = false;
    } else if (conn->disconnectpending) {
      pni_raw_put_event(conn, PN_RAW_CONNECTION_DISCONNECTED);
      conn->disconnectpending = false;
    } else if (!conn->wclosed && !conn->wbuffer_first_towrite && !conn->wneedbufferevent) {
      // Ran out of write buffers
      pni_raw_put_event(conn, PN_RAW_CONNECTION_NEED_WRITE_BUFFERS);
      conn->wneedbufferevent = true;
    } else if (!conn->rclosed && !conn->rbuffer_first_unused && !conn->rneedbufferevent) {
      // Ran out of read buffers
      pni_raw_put_event(conn, PN_RAW_CONNECTION_NEED_READ_BUFFERS);
      conn->rneedbufferevent = true;
    } else {
      return NULL;
    }
  } while (true);
}

void pni_raw_close(pn_raw_connection_t *conn) {
  // TODO: Do we need different flags here?
  // TODO: What is the precise semantics for close?
  bool rclosed = conn->rclosed;
  if (!rclosed) {
    conn->rclosed = true;
    conn->rclosedpending = true;
  }
  bool wclosed = conn->wclosed;
  if (!wclosed) {
    if (conn->wbuffer_first_towrite) {
      conn->wdraining = true;
    } else {
      conn->wclosed = true;
      conn->wclosedpending = true;
    }
  }
  if ((!rclosed || !wclosed) && !conn->wdraining) {
    pni_raw_disconnect(conn);
  }
}

bool pn_raw_connection_is_read_closed(pn_raw_connection_t *conn) {
  assert(conn);
  return conn->rclosed;
}

bool pn_raw_connection_is_write_closed(pn_raw_connection_t *conn) {
  assert(conn);
  return conn->wclosed || conn->wdraining;
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
