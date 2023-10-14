/*
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
 */

#include "thread.h"

#include <proton/annotations.h>
#include <proton/raw_connection.h>
#include <proton/tls.h>
#include <proton/listener.h>
#include <proton/netaddr.h>
#include <proton/proactor.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>


/*
 * Jabberwock raw connection example with and without TLS.
 *
 * One client and one server take turns sending a line of the poem.
 * The simple "application" logic resides in line_of_jabber() and gobble_jabber().
 * handle_outgoing() and handle_incoming() handle the application IO
 * plumbing to use Proton raw connections, with or without TLS.
 *
 * See the "no_tls" option to contrast the approach compared to TLS usage.
 *
 * This example is frugal in the number of buffers it uses, giving at most one
 * pn_raw_buffer_t at any time to the read or write sides of raw connections and
 * the TLS engine.
 *
 * In this example orderly termination of the connection is initiated by one side.
 * The initiator does not wait for any close handshake from the peer.
 * The peer looks for confirmation of orderly closure from the initiator.
 *
 * Based on the broker and raw connection examples.  Must be able to find
 * the TLS certificates in the same location as the broker example.
 *
 * The astute reader will realize that the lack of any locking mechanisms
 * works only by luck of the simple nature of this "taking turns" example
 * and may not survive TSAN scrutiny.
 */


/* The ssl-certs subdir must be in the current directory for TLS configuration */
#define SSL_FILE(NAME) "ssl-certs/" NAME
#define SSL_PW "tserverpw"
/* Windows vs. OpenSSL certificates */
#if defined(_WIN32)
#  define CERTIFICATE(NAME) SSL_FILE(NAME "-certificate.p12")
#  define SET_CREDENTIALS(DOMAIN, NAME)                                 \
  pn_tls_config_set_credentials(DOMAIN, SSL_FILE(NAME "-full.p12"), "", SSL_PW)
#else
#  define CERTIFICATE(NAME) SSL_FILE(NAME "-certificate.pem")
#  define SET_CREDENTIALS(DOMAIN, NAME)                                 \
  pn_tls_config_set_credentials(DOMAIN, CERTIFICATE(NAME), SSL_FILE(NAME "-private-key.pem"), SSL_PW)
#endif

static void jfatal(const char *file, int line) {
  fprintf(stderr, "epoll proactor failure in %s:%d\n", __FILE__, __LINE__);
  abort();                                      \
}
#define jcheck( EXPR )                                     \
  { if (!(EXPR)) jfatal(__FILE__,__LINE__); }

static uintptr_t JBR_UNUSED = 0;
static uintptr_t JBR_INUSE = 1;

// A raw buffer "pool", in name only
static void rbuf_pool_get(pn_raw_buffer_t *buf) {
  memset(buf, 0, sizeof(pn_raw_buffer_t));
  buf->bytes = calloc(1, 4096);
  buf->capacity = 4096;
  buf->context = JBR_UNUSED;
}

static void rbuf_pool_return(pn_raw_buffer_t *rbuf) {
  free(rbuf->bytes);
}

// Buffer is "in use" if ownership transferred to raw connection or TLS until a future event.
static void buf_set_in_use(pn_raw_buffer_t *rbuf, bool in_use) {
  rbuf->context = in_use ? JBR_INUSE : JBR_UNUSED;
}
static bool buf_in_use(pn_raw_buffer_t *rbuf) {
  return rbuf->context == JBR_INUSE;
}
static bool buf_unused(pn_raw_buffer_t *rbuf) {
  return !buf_in_use(rbuf);
}

static void buf_reset(pn_raw_buffer_t *rbuf) {
  rbuf->size = 0;
  rbuf->offset = 0;
  memset(rbuf->bytes, 0, rbuf->capacity);
  buf_set_in_use(rbuf, false);
}

static size_t size_t_min(size_t a, size_t b) {
  return (a < b) ? a : b;
}

typedef struct jabber_t {
  pn_proactor_t *proactor;
  size_t threads;
  pn_tls_config_t *srv_domain;
  pn_tls_config_t *cli_domain;
  const char *host;
  const char *port;
  pn_listener_t *listener;
  size_t current_jline;
  size_t total_bytes_sent;
  size_t total_bytes_recv;
  bool alpn_enabled;
} jabber_t;

typedef struct jabber_connection_t {
  jabber_t *parent;
  pn_raw_connection_t *rawc;
  pn_tls_t *tls;
  // Orderly close:
  // Non TLS: shutdown write side, read side detects EOF
  // TLS: same shutdown, but also an encrypted TLS EOF using pn_tls_close_output()
  //      which sends the TLS protocol close_notify alert record.
  bool need_rawc_write_close;
  bool tls_has_output;

  bool connecting;
  bool tls_closing;
  bool input_done;
  bool orderly_close_initiated;
  bool orderly_close_detected;
  bool tls_error;

  pn_raw_buffer_t out_wire_buf;
  pn_raw_buffer_t in_wire_buf;
  pn_raw_buffer_t out_app_buf;
  pn_raw_buffer_t in_app_buf;
  bool is_server;
  bool jabber_turn;
  char *alpn_protocol;
} jabber_connection_t;


static inline uint32_t room(pn_raw_buffer_t const *rb) {
  if (rb)
    return rb->capacity - (rb->offset + rb->size);
  return 0;
}

static const char* jlines[] = {
                               "Twas brillig, and the slithy toves",
                               "Did gire and gymble in the wabe.",
                               "All mimsy were the borogroves,",
                               "And the mome raths outgrabe."
};

static size_t jlines_count = sizeof(jlines) / sizeof(jlines[0]);

// Provide one line of poem into rbuf.
static void line_of_jabber(jabber_connection_t *jc, pn_raw_buffer_t *rbufp) {
  jabber_t *j = jc->parent;
  const char *self = jc->is_server ? "server" : "client";
  if (j->current_jline < jlines_count) {
    size_t len = strlen(jlines[j->current_jline]);
    jcheck( len < room(rbufp) );
    memcpy(rbufp->bytes + rbufp->offset, jlines[j->current_jline++], len);
    rbufp->size = len;
    j->total_bytes_sent += len;
  }
}

// Consume content of poem sent by peer.
static void gobble_jabber(jabber_connection_t* jc, pn_raw_buffer_t* rbuf) {
  jabber_t *j = jc->parent;
  const char *self = jc->is_server ? "server" : "client";
  jcheck( rbuf->size != 0 );
  printf("<--  %s received:  %.4096s\n", self, rbuf->bytes + rbuf->offset);

  j->total_bytes_recv += rbuf->size;
  if (j->total_bytes_recv == j->total_bytes_sent) {
    if (j->current_jline != jlines_count) {
      // This connection sends the next set of lines.
      jc->jabber_turn = true;
      pn_raw_connection_wake(jc->rawc);
    }
  }
}

// Return false if TLS error encountered.
static bool jabber_tls_process(jabber_connection_t* jc) {
  int err = pn_tls_process(jc->tls);
  if (err && !jc->tls_error) {
    jc->tls_error = true;
    char buf[256];
    pn_tls_get_session_error_string(jc->tls, buf, sizeof(buf));
    fprintf(stderr, "TLS processing error: %s\n", buf);
    fprintf(stderr, "Jabber %s connection terminated.\n", jc->is_server ? "server" : "client");
    // Stop all application data processing.
    // Close input.  Continue non-application output in case we have a TLS protocol error to send to peer.
    pn_raw_connection_read_close(jc->rawc);
    jc->input_done = true;
    jc->tls_has_output = pn_tls_is_encrypt_output_pending(jc->tls) > 0;
    return false;
  }
  return true;
}

// TLS close requires best efforts to send final closure or error alert record.
// Peer is not obligated to wait for it.
static void jabber_tls_begin_close(jabber_connection_t* jc) {
  if (!jc->tls_closing) {
    jc->tls_closing = true;
    pn_tls_close_output(jc->tls);
    jc->need_rawc_write_close = true;  // Remember to eventually close the raw connection.
    pn_tls_process(jc->tls);           // Best efforts. No error check.
    pn_raw_connection_wake(jc->rawc);  // Ensure handle_outgoing is called.
  }
}

static void check_alpn(jabber_connection_t* jc) {
  const char *self = jc->is_server ? "server" : "client";

  if (!jc->alpn_protocol) {
    const char *protocol_name;
    size_t len;
    if (pn_tls_get_alpn_protocol(jc->tls, &protocol_name, &len)) {
      jc->alpn_protocol = (char *) malloc(len+1);
      jcheck( jc->alpn_protocol );
      memmove(jc->alpn_protocol, protocol_name, len);
      jc->alpn_protocol[len] = 0;
      printf("**%s: using ALPN protocol %s\n", self, jc->alpn_protocol);
    } else {
      printf("**%s: no available ALPN protocol\n", self);
    }
  }
}

static void load_alpn_strings(pn_tls_config_t *cli_domain, pn_tls_config_t *srv_domain) {
  const char *protos[4];
  // server...
  protos[0] = "jibberjabber";
  protos[1] = "jabber/v1";   // expected winner
  protos[2] = "piglatin";
  pn_tls_config_set_alpn_protocols(srv_domain, protos, 3);
  //client
  protos[0] = "jabber/v2";
  protos[1] = "piglatin";
  protos[2] = "jabber/v1";   // expected winner
  protos[3] = "ghost";
  pn_tls_config_set_alpn_protocols(cli_domain, protos, 4);
}

static void handle_outgoing(jabber_connection_t* jc) {
  // For this simplified example, 1 buffer of application output is dealt
  // with at a time.

  jabber_t *j = jc->parent;
  if (buf_in_use(&jc->out_wire_buf)) {
    // See if raw connection done with the buffer
    if (1 == pn_raw_connection_take_written_buffers(jc->rawc, &jc->out_wire_buf, 1)) {
      buf_reset(&jc->out_wire_buf);
    } else {
      return;  // Nothing to do until notified of future raw connection write completion
    }
  }

  if (!jc->jabber_turn && !jc->tls_has_output && !jc->need_rawc_write_close)
    return;  // nothing to send at this time

  if (!jc->tls) {
    line_of_jabber(jc, &jc->out_wire_buf);
    if (jc->out_wire_buf.size > 0) {
      jc->jabber_turn = false;    // Peer gets to do next jabber.
      jcheck( pn_raw_connection_write_buffers(jc->rawc, &jc->out_wire_buf, 1) == 1 );
      buf_set_in_use(&jc->out_wire_buf, true);
    }
    // If no more application data to write, start orderly close.
    if (j->current_jline == jlines_count) {
      pn_raw_connection_write_close(jc->rawc);
      jc->orderly_close_initiated = true;
    }
  } else {
    // TLS

    // Reacquire encryption buffer if TLS library is done with it.
    if (buf_in_use(&jc->out_app_buf) && pn_tls_take_encrypt_input_buffers(jc->tls, &jc->out_app_buf, 1) == 1)
      buf_reset(&jc->out_app_buf);

    if (pn_tls_is_secure(jc->tls) && jc->jabber_turn && !jc->tls_closing) {
      jcheck( jc->alpn_protocol || !jc->parent->alpn_enabled );
      // Add jabber data if there is room.
      if (buf_unused(&jc->out_app_buf)) {
        line_of_jabber(jc, &jc->out_app_buf);
        if (jc->out_app_buf.size > 0) {
          jc->jabber_turn = false;  // Peer gets to do next jabber.
          buf_set_in_use(&jc->out_app_buf, true);
          size_t consumed = pn_tls_give_encrypt_input_buffers(jc->tls, &jc->out_app_buf, 1);
          if (consumed != 1) abort();

          // If no more application data to write, start orderly close.
          // For TLS, indicate we are done writing to generate the protocol's EOS (closure alert).
          if (j->current_jline == jlines_count) {
            jc->orderly_close_initiated = true;
            jabber_tls_begin_close(jc);
          }

          jabber_tls_process(jc);
        }
      }
    }

    // Even if no application output just queued for processing (i.e. no call to
    // pn_tls_give_encrypt_input_buffers()), there may be straggling encrypted output.
    // Also possible we are flushing TLS error information to peer.
    if (buf_unused(&jc->out_wire_buf) && pn_tls_need_encrypt_output_buffers(jc->tls)) {
      pn_tls_give_encrypt_output_buffers(jc->tls, &jc->out_wire_buf, 1);
      jabber_tls_process(jc);
      jcheck( pn_tls_take_encrypt_output_buffers(jc->tls, &jc->out_wire_buf, 1) == 1 );

      // Write the application data we just encrypted.
      jcheck( pn_raw_connection_write_buffers(jc->rawc, &jc->out_wire_buf, 1) == 1);
      buf_set_in_use(&jc->out_wire_buf, true);
    }

    // Remember whether we missed some output buffered in the TLS library and need to come back.
    jc->tls_has_output = pn_tls_is_encrypt_output_pending(jc->tls) > 0;

    if (jc->need_rawc_write_close && !jc->tls_has_output) {
      // We have written to the raw connection all the output the TLS library can ever give us.
      pn_raw_connection_write_close(jc->rawc);
      jc->need_rawc_write_close = false;
    }
  }
}

static void handle_incoming(jabber_connection_t* jc, bool rawc_input_closed_event) {
  // There may be more received wire buffers than sent.
  if (jc->input_done)
    return;
  bool wire_bytes = false;
  const char *self = jc->is_server ? "server" : "client";

  if (!rawc_input_closed_event) {
    if (!buf_in_use(&jc->in_wire_buf) ||
        pn_raw_connection_take_read_buffers(jc->rawc, &jc->in_wire_buf, 1) != 1)
      return;  // No wire buffer to process - come back later.
    if (jc->in_wire_buf.size > 0)
      wire_bytes = true;
  }

  if (!jc->tls) {
    if (wire_bytes) {
      // Use wire data directly
      gobble_jabber(jc, &jc->in_wire_buf);
      buf_reset(&jc->in_wire_buf);
    } else {
      if (!jc->orderly_close_initiated && jc->parent->current_jline == jlines_count)
        jc->orderly_close_detected = true;
      // Whether unexpected hangup or expected EOS, Jabber has no more data to send.
      pn_raw_connection_close(jc->rawc);
      jc->input_done = true;
    }
  } else {
    // TLS case
    if (wire_bytes) {
      jcheck( pn_tls_get_decrypt_input_buffer_capacity(jc->tls) > 0 );
      size_t consumed = pn_tls_give_decrypt_input_buffers(jc->tls, &jc->in_wire_buf, 1);
      if (consumed == 0) {
        if (pn_tls_is_input_closed(jc->tls))
          // Library will not process anything after TLS EOS, and we are not expecting any trailing data.
          printf("data after TLS EOF\n"); // Error by peer or garbage bytes from attacker.
        else
          printf("Unexpected TLS decrypt input buffer failure\n");
        abort();
      }

      if (!jabber_tls_process(jc))
        return;

      if (jc->parent->alpn_enabled && !jc->alpn_protocol)
        check_alpn(jc);

      while (pn_tls_need_decrypt_output_buffers(jc->tls)) {
        jcheck( pn_tls_give_decrypt_output_buffers(jc->tls, &jc->in_app_buf, 1) == 1 );
        if (!jabber_tls_process(jc))
          return;
        jcheck( pn_tls_take_decrypt_output_buffers(jc->tls, &jc->in_app_buf, 1) == 1 );
        jcheck( jc->in_app_buf.size > 0 );
        gobble_jabber(jc, &jc->in_app_buf);  // Application layer consumes all bytes.
        buf_reset(&jc->in_app_buf);
      }

      // Reclaim and recycle the TLS encoded input buffer (from the wire) processed by the TLS library.
      // If a partial TLS record remains, it is buffered in the TLS engine.
      jcheck( pn_tls_take_decrypt_input_buffers(jc->tls, &jc->in_wire_buf, 1) == 1);
      buf_reset(&jc->in_wire_buf);

      if (pn_tls_is_input_closed(jc->tls) && !jc->orderly_close_initiated) {
        printf("**%s: received TLS end of session notification from peer\n", self);
        jc->orderly_close_detected = true;
        jabber_tls_begin_close(jc);
      }
    } else {
      // EOS
      jc->input_done = true;
      if (!pn_tls_is_input_closed(jc->tls) && !jc->orderly_close_initiated)
        // Expecting graceful close with peer in this example.
        printf("**%s: TLS session ended abruptly\n", self);
      jabber_tls_begin_close(jc);
    }
    // TLS input can generate non-application output.  Note that here.
    jc->tls_has_output = pn_tls_is_encrypt_output_pending(jc->tls) > 0;
  }
}

static void allocate_wire_buffers(jabber_connection_t *jc) {
  rbuf_pool_get(&jc->out_wire_buf);
  rbuf_pool_get(&jc->in_wire_buf);
}

static void allocate_tls_buffers(jabber_connection_t *jc) {
  pn_tls_set_encrypt_input_buffer_max_capacity(jc->tls, 1);
  pn_tls_set_decrypt_input_buffer_max_capacity(jc->tls, 1);
  pn_tls_set_encrypt_output_buffer_max_capacity(jc->tls, 1);
  pn_tls_set_decrypt_output_buffer_max_capacity(jc->tls, 1);
  rbuf_pool_get(&jc->in_app_buf);
  rbuf_pool_get(&jc->out_app_buf);
}

static void create_client_connection(jabber_t *j) {
  char addr[PN_MAX_ADDR];
  pn_raw_connection_t *c = pn_raw_connection();
  jabber_connection_t *jc = calloc(sizeof(*jc), 1);
  jc->rawc = c;
  jc->is_server = false;
  jc->jabber_turn = true;  // Client goes first for purposes of this example.  Arbitrary choice.
  jc->parent = j;
  pn_raw_connection_set_context(c, (void *) jc);

  // Client side TLS can be set up anywhere between here and first write (TLS clienthello).
  if (j->cli_domain) {
    jc->tls = pn_tls(j->cli_domain);
    pn_tls_set_peer_hostname(jc->tls, "test_server");
    allocate_tls_buffers(jc);
    pn_tls_start(jc->tls);
    jc->tls_has_output = true; // always true for initial client side TLS.
  }

  allocate_wire_buffers(jc);
  pn_proactor_addr(addr, sizeof(addr), j->host, j->port);
  pn_proactor_raw_connect(j->proactor, c, addr);
}

static void create_server_connection(jabber_t *j, pn_listener_t *listener) {
  pn_raw_connection_t *c = pn_raw_connection();
  jabber_connection_t *jc = calloc(sizeof(*jc), 1);
  jc->rawc = c;
  jc->is_server = true;
  jc->parent = j;
  pn_raw_connection_set_context(c, (void *) jc);

  // Server side TLS can be set up anywhere between here and first read (TLS clienthello).
  if (j->srv_domain) {
    jc->tls = pn_tls(j->srv_domain);
    allocate_tls_buffers(jc);
    pn_tls_start(jc->tls);
  }
  allocate_wire_buffers(jc);
  pn_listener_raw_accept(listener, c);
  printf("**listener accepted %p\n", (void *)jc);
  pn_listener_close(j->listener); // Single client.
}

// Called on disconnect.  No more raw connection IO in either direction.
static void tls_cleanup(jabber_connection_t* jc) {
  if (jc->tls) {
    pn_tls_stop(jc->tls);
    pn_raw_buffer_t rb;
    if (buf_in_use(&jc->out_app_buf))
      jcheck( pn_tls_take_encrypt_input_buffers(jc->tls, &jc->out_app_buf, 1) == 1 );
    jcheck( buf_unused(&jc->in_app_buf) );
    rbuf_pool_return(&jc->in_app_buf);
    rbuf_pool_return(&jc->out_app_buf);
    free(jc->alpn_protocol);
    pn_tls_free(jc->tls);
    jc->tls = NULL;
  }
}

static bool handle_raw_connection(jabber_connection_t* jc, pn_event_t* event) {
  switch (pn_event_type(event)) {

    case PN_RAW_CONNECTION_CONNECTED: {
      printf("**raw connection established %p %s\n", (void *)jc, jc->is_server ? "server" : "client");
      jc->connecting = false;
    } break;

    case PN_RAW_CONNECTION_NEED_READ_BUFFERS: {
      jcheck( buf_unused(&jc->in_wire_buf) );
      pn_raw_connection_give_read_buffers(jc->rawc, &jc->in_wire_buf, 1);
      buf_set_in_use(&jc->in_wire_buf, true);
    } break;

    case PN_RAW_CONNECTION_CLOSED_WRITE: {
      pn_raw_connection_close(jc->rawc);  // In case not fully closed
    } break;

    case PN_RAW_CONNECTION_WRITTEN:
    case PN_RAW_CONNECTION_NEED_WRITE_BUFFERS:
    case PN_RAW_CONNECTION_WAKE: {
      handle_outgoing(jc);
    } break;

    case PN_RAW_CONNECTION_CLOSED_READ:
    case PN_RAW_CONNECTION_READ: {
      handle_incoming(jc, pn_event_type(event) == PN_RAW_CONNECTION_CLOSED_READ);
      if (jc->tls_has_output)
        handle_outgoing(jc);
    } break;

    case PN_RAW_CONNECTION_DRAIN_BUFFERS: {
      pn_raw_buffer_t rbuf;
      while (1 == pn_raw_connection_take_read_buffers(jc->rawc, &rbuf, 1)) {
      }
      while (1 == pn_raw_connection_take_written_buffers(jc->rawc, &rbuf, 1)) {
      }
    } break;

    case PN_RAW_CONNECTION_DISCONNECTED: {
      const char *self = jc->is_server ? "Server" : "Client";
      pn_condition_t *cond = pn_raw_connection_condition(jc->rawc);
      if (jc->connecting)
        printf("**Connection failed\n");
      else {
        if (!(jc->orderly_close_initiated || jc->orderly_close_detected) || pn_condition_is_set(cond))
          printf("**%s connection terminated abnormally\n", self);
        else if (jc->orderly_close_detected && jc->parent->current_jline != jlines_count)
          printf("**Jabber completed successfully\n", self);
      }
      if (pn_condition_is_set(cond)) {
        fprintf(stderr, "raw connection failure: %s\n", pn_condition_get_name(cond), pn_condition_get_description(cond));
      }

      if (jc->tls) {
        if (jc->need_rawc_write_close)
          printf("**%s connection terminated with unsent TLS data\n", self);
        tls_cleanup(jc);
      }
      rbuf_pool_return(&jc->in_wire_buf);
      rbuf_pool_return(&jc->out_wire_buf);
      free(jc);
    } break;
  }
  return true;
}

static bool handle(jabber_t* j, pn_event_t* event) {
  switch (pn_event_type(event)) {

    case PN_LISTENER_OPEN: {
      create_client_connection(j);
      break;
    }
    case PN_LISTENER_ACCEPT: {
      pn_listener_t *listener = pn_event_listener(event);
      create_server_connection(j, listener);
    } break;

    case PN_PROACTOR_INACTIVE:
      printf("**proactor inactive: connections and listeners finalized\n");
      PN_FALLTHROUGH;
    case PN_PROACTOR_INTERRUPT: {
      pn_proactor_t *proactor = pn_event_proactor(event);
      pn_proactor_interrupt(proactor);
      return false;
    } break;

    default: {
      pn_raw_connection_t *c = pn_event_raw_connection(event);
      if (c) {
        jabber_connection_t *jc = (jabber_connection_t *) pn_raw_connection_get_context(c);
        handle_raw_connection(jc, event);
      }
    }
  }
  return true;
}

static void* j_thread(void *void_j) {
  jabber_t *j = (jabber_t*)void_j;
  bool finished = false;
  do {
    pn_event_batch_t *events = pn_proactor_wait(j->proactor);
    pn_event_t *e;
    while ((e = pn_event_batch_next(events))) {
      if (!handle(j, e)) finished = true;
    }
    pn_proactor_done(j->proactor, events);
  } while(!finished);
  return NULL;
}

// main is from broker.c but using raw connections instead of AMQP connections.
int main(int argc, char **argv) {
  int err;
  srand(time(NULL));
  jabber_t j = {0};
  j.host = (argc > 1) ? argv[1] : "";
  j.port = (argc > 2) ? argv[2] : "15243";  // our default Jabberwock address
  j.listener = pn_listener();
  bool use_tls = true;
  if (argc > 3 && strncmp(argv[3], "no_tls", 6) == 0)
    use_tls = false;
  j.alpn_enabled = use_tls;  // make configurable

  j.proactor = pn_proactor();
  j.threads = 4;
  if (use_tls) {
    j.srv_domain = pn_tls_config(PN_TLS_MODE_SERVER);
    if (SET_CREDENTIALS(j.srv_domain, "tserver") != 0) {
      printf("Failed to set up server certificate: %s, private key: %s\n", CERTIFICATE("tserver"), SSL_FILE("tserver-private-key.pem"));
      exit(1);
    }
    j.cli_domain = pn_tls_config(PN_TLS_MODE_CLIENT); //  PN_TLS_VERIFY_PEER_NAME by default
    if (pn_tls_config_set_trusted_certs(j.cli_domain, CERTIFICATE("tserver")) != 0) {
      printf("CA failure\n");
      exit(1);
    }

    if (j.alpn_enabled) {
      // Set some ALPN values for each peer.
      load_alpn_strings(j.cli_domain, j.srv_domain);
    }
  }
  {
  /* Listen on addr */
  char addr[PN_MAX_ADDR];
  pn_proactor_addr(addr, sizeof(addr), j.host, j.port);
  pn_proactor_listen(j.proactor, j.listener, addr, 16);
  }

  {
  /* Start n-1 threads */
  pthread_t* threads = (pthread_t*)calloc(sizeof(pthread_t), j.threads);
  size_t i;
  for (i = 0; i < j.threads-1; ++i) {
    pthread_create(&threads[i], NULL, j_thread, &j);
  }
  j_thread(&j);            /* Use the main thread too. */
  /* Join the other threads */
  for (i = 0; i < j.threads-1; ++i) {
    pthread_join(threads[i], NULL);
  }
  pn_proactor_free(j.proactor);
  free(threads);
  if (j.srv_domain)
    pn_tls_config_free(j.srv_domain);
  if (j.cli_domain)
    pn_tls_config_free(j.cli_domain);
  return 0;
  }
}
