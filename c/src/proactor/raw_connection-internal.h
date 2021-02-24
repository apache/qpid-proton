#ifndef PROACTOR_RAW_CONNECTION_INTERNAL_H
#define PROACTOR_RAW_CONNECTION_INTERNAL_H
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

#ifdef __cplusplus
extern "C" {
#endif

enum {
  read_buffer_count = 16,
  write_buffer_count = 16
};

typedef enum {
  buff_rempty    = 0,
  buff_unread    = 1,
  buff_read      = 2,
  buff_wempty    = 4,
  buff_unwritten = 5,
  buff_written   = 6
} buff_type;

/*
 * r = read, w = write
 * init = initial
 * o = open - can read/write
 * d = draining - write draining
 * c = closing - shutdown pending
 * s = stopped (closed)
 */
typedef enum {
  conn_wrong = -1,
  conn_init  = 0,
  conn_ro_wo = 1,
  conn_ro_wd = 2,
  conn_ro_wc = 3,
  conn_ro_ws = 4,
  conn_rc_wo = 5,
  conn_rc_wd = 6,
  conn_rs_wo = 7,
  conn_rs_wd = 8,
  conn_rs_ws = 9,
  conn_fini  = 10,
} raw_conn_state;

typedef enum {
  disc_init        = 0,
  disc_drain_msg   = 1,
  disc_read_msg    = 2,
  disc_written_msg = 3,
  disc_fini        = 4
} raw_disconnect_state;

typedef uint16_t buff_ptr; // This is always the index+1 so that 0 can be special

typedef struct pbuffer_t {
  uintptr_t context;
  char *bytes;
  uint32_t capacity;
  uint32_t size;
  uint32_t offset;
  buff_ptr next;
  uint8_t type; // For debugging
} pbuffer_t;

struct pn_raw_connection_t {
  pbuffer_t rbuffers[read_buffer_count];
  pbuffer_t wbuffers[write_buffer_count];
  pn_condition_t *condition;
  pn_collector_t *collector;
  pn_record_t *attachments;
  uint32_t unwritten_offset;
  uint16_t rbuffer_count;
  uint16_t wbuffer_count;

  buff_ptr rbuffer_first_empty;
  buff_ptr rbuffer_first_unused;
  buff_ptr rbuffer_last_unused;
  buff_ptr rbuffer_first_read;
  buff_ptr rbuffer_last_read;

  buff_ptr wbuffer_first_empty;
  buff_ptr wbuffer_first_towrite;
  buff_ptr wbuffer_last_towrite;
  buff_ptr wbuffer_first_written;
  buff_ptr wbuffer_last_written;

  uint8_t state; // really raw_conn_state
  uint8_t disconnect_state; // really raw_disconnect_state

  bool rrequestedbuffers;
  bool wrequestedbuffers;

  bool rpending;
  bool wpending;
  bool rclosedpending;
  bool wclosedpending;
  bool disconnectpending;
  bool wakepending;
};

/*
 * Raw connection internal API
 */
bool pni_raw_validate(pn_raw_connection_t *conn);
void pni_raw_connected(pn_raw_connection_t *conn);
void pni_raw_connect_failed(pn_raw_connection_t *conn);
void pni_raw_wake(pn_raw_connection_t *conn);
void pni_raw_close(pn_raw_connection_t *conn);
void pni_raw_read_close(pn_raw_connection_t *conn);
void pni_raw_write_close(pn_raw_connection_t *conn);
void pni_raw_read(pn_raw_connection_t *conn, int sock, long (*recv)(int, void*, size_t), void (*set_error)(pn_raw_connection_t *, const char *, int));
void pni_raw_write(pn_raw_connection_t *conn, int sock, long (*send)(int, const void*, size_t), void (*set_error)(pn_raw_connection_t *, const char *, int));
void pni_raw_process_shutdown(pn_raw_connection_t *conn, int sock, int (*shutdown_rd)(int), int (*shutdown_wr)(int));
bool pni_raw_can_read(pn_raw_connection_t *conn);
bool pni_raw_can_write(pn_raw_connection_t *conn);
pn_event_t *pni_raw_event_next(pn_raw_connection_t *conn);
void pni_raw_initialize(pn_raw_connection_t *conn);
void pni_raw_finalize(pn_raw_connection_t *conn);

#ifdef __cplusplus
}
#endif

#endif // PROACTOR_RAW_CONNECTION_INTERNAL_H
