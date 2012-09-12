#ifndef PROTON_SRC_DRIVER_INTERNAL_H
#define PROTON_SRC_DRIVER_INTERNAL_H 1
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


/* Decls */

struct pn_driver_t {
  pn_error_t *error;
  pn_listener_t *listener_head;
  pn_listener_t *listener_tail;
  pn_listener_t *listener_next;
  pn_connector_t *connector_head;
  pn_connector_t *connector_tail;
  pn_connector_t *connector_next;
  size_t listener_count;
  size_t connector_count;
  size_t closed_count;
  int ctrl[2]; //pipe for updating selectable status
  pn_trace_t trace;

  struct pn_driver_poller_t   *poller;
};

int pn_driver_poller_init( struct pn_driver_t * );
void pn_driver_poller_destroy( struct pn_driver_t * );


struct pn_listener_t {
  pn_driver_t *driver;
  pn_listener_t *listener_next;
  pn_listener_t *listener_prev;
  bool pending;
  int fd;
  void *context;

  struct pn_listener_poller_t *poller;
  struct pn_listener_ssl_t *ssl;
};

int pn_listener_poller_init( struct pn_listener_t *);
void pn_listener_poller_destroy( struct pn_listener_t *);


#define PN_CONNECTOR_IO_BUF_SIZE (4*1024)
#define PN_CONNECTOR_NAME_MAX (256)
#define PN_SEL_RD (0x0001)
#define PN_SEL_WR (0x0002)


struct pn_connector_t {
  pn_driver_t *driver;
  pn_connector_t *connector_next;
  pn_connector_t *connector_prev;
  char name[PN_CONNECTOR_NAME_MAX];
  bool pending_tick;
  bool pending_read;
  bool pending_write;
  int fd;
  int status;
  pn_trace_t trace;
  bool closed;
  time_t wakeup;
  void (*read)(pn_connector_t *);
  void (*write) (pn_connector_t *);
  time_t (*tick)(pn_connector_t *sel, time_t now);

  int (*io_handler)(pn_connector_t *);

  size_t input_size;
  char input[PN_CONNECTOR_IO_BUF_SIZE];
  bool input_eos;
  size_t output_size;
  char output[PN_CONNECTOR_IO_BUF_SIZE];
  pn_connection_t *connection;
  pn_transport_t *transport;
  pn_sasl_t *sasl;
  bool input_done;
  bool output_done;
  pn_listener_t *listener;
  void *context;

  struct pn_connector_poller_t *poller;
  struct pn_connector_ssl_t *ssl;
};

int pn_connector_poller_init( struct pn_connector_t *);
void pn_connector_poller_destroy( struct pn_connector_t *);
void pn_driver_poller_wait(struct pn_driver_t *, int timeout_ms);
int pn_io_handler(pn_connector_t *);
int pn_null_io_handler(pn_connector_t *);
void pn_connector_process_output(pn_connector_t *);
void pn_connector_process_input(pn_connector_t *);


#endif /* driver-internal.h */
