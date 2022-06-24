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

#include <proton/tls.h>

#include "pn_test.hpp"

#ifdef _WIN32
#include <errno.h>
#else
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#endif

#include <cstring>

using namespace pn_test;
using Catch::Matchers::Contains;
using Catch::Matchers::Equals;

/* Note must be run in the current directory to find certificate files */
#define SSL_FILE(NAME) "ssl-certs/" NAME
#define SSL_PW(NAME) NAME "pw"
/* Windows vs. OpenSSL certificates */
#if defined(_WIN32)
#  define CERTIFICATE(NAME) SSL_FILE(NAME "-certificate.p12")
#  define SET_CREDENTIALS(DOMAIN, NAME)                                 \
  pn_tls_config_set_credentials(DOMAIN, SSL_FILE(NAME "-full.p12"), "", SSL_PW(NAME))
#else
#  define CERTIFICATE(NAME) SSL_FILE(NAME "-certificate.pem")
#  define SET_CREDENTIALS(DOMAIN, NAME)                                 \
  pn_tls_config_set_credentials(DOMAIN, CERTIFICATE(NAME), SSL_FILE(NAME "-private-key.pem"), SSL_PW(NAME))
#endif

static void reset_rbuf(pn_raw_buffer_t *rb) {
  memset(rb, 0, sizeof(*rb));
}

static void set_rbuf(pn_raw_buffer_t *rb, char * bytes, uint32_t capacity, uint32_t size) {
  rb->bytes = bytes;
  rb->capacity = capacity;
  rb->size = size;
  rb->offset = 0;
}

TEST_CASE("handshake and data") {
  static char cli_data[] = {"sample client data (request)"};
  static char srv_data[] = {"sample server data (response)"};
  pn_tls_config_t *client_config = pn_tls_config(PN_TLS_MODE_CLIENT);
  REQUIRE( client_config );
  pn_tls_config_t *server_config = pn_tls_config(PN_TLS_MODE_SERVER);
  REQUIRE( server_config );

  REQUIRE( pn_tls_config_set_trusted_certs(client_config, CERTIFICATE("tserver")) == 0 );
  REQUIRE(SET_CREDENTIALS(server_config, "tserver") == 0);

  pn_tls_t *cli_tls = pn_tls(client_config);
  pn_tls_set_peer_hostname(cli_tls, "test_server");
  pn_tls_t *srv_tls = pn_tls(server_config);

  CHECK( cli_tls != NULL );
  CHECK( srv_tls != NULL );

  // Config complete on both sides.
  REQUIRE( pn_tls_start(cli_tls) == 0 );
  REQUIRE( pn_tls_start(srv_tls) == 0 );

  char wire_bytes[4096]; // encrypted data, sent between client and server
  char app_bytes[4096];  // plain text data read or written at either peer
  pn_raw_buffer_t app_buf, wire_buf, rb_array[2];
  reset_rbuf(&wire_buf);
  set_rbuf(&wire_buf, wire_bytes, sizeof(wire_bytes), 0);
  reset_rbuf(&app_buf);
  set_rbuf(&app_buf, app_bytes, sizeof(app_bytes), 0);

  /* client hello part 1: client side */

  REQUIRE( pn_tls_need_encrypt_output_buffers(cli_tls) == true );
  REQUIRE( pn_tls_need_encrypt_output_buffers(srv_tls) == false );

  pn_tls_give_encrypt_output_buffers(cli_tls, &wire_buf, 1);
  REQUIRE( pn_tls_process(cli_tls) == 0 );
  REQUIRE( pn_tls_take_encrypt_output_buffers(cli_tls, rb_array, 2) == 1 );
  REQUIRE( rb_array[0].bytes == wire_bytes );
  REQUIRE( rb_array[0].size < sizeof(wire_bytes) );  // Strictly less expected.

  /* client hello part 2: server side */

  REQUIRE( pn_tls_need_encrypt_output_buffers(srv_tls) == false );
  REQUIRE( pn_tls_give_decrypt_input_buffers(srv_tls, rb_array, 1) == 1 );
  REQUIRE( pn_tls_process(srv_tls) == 0 );
  REQUIRE( pn_tls_need_decrypt_output_buffers(srv_tls) == false ); // nothing yet
  REQUIRE( pn_tls_take_decrypt_input_buffers(srv_tls, rb_array, 2) == 1 );
  REQUIRE( rb_array[0].bytes == wire_bytes );

  /* server hello part1: server side */
  REQUIRE( pn_tls_is_encrypt_output_pending(srv_tls) == true );
  set_rbuf(&wire_buf, wire_bytes, sizeof(wire_bytes), 0);
  REQUIRE( pn_tls_give_encrypt_output_buffers(srv_tls, &wire_buf, 1) == 1 );
  REQUIRE( pn_tls_process(srv_tls) == 0 );
  REQUIRE( pn_tls_take_encrypt_output_buffers(srv_tls, rb_array, 2) == 1 );
  REQUIRE( rb_array[0].bytes == wire_bytes );
  REQUIRE( rb_array[0].size < sizeof(wire_bytes) );  // Strictly less expected.

  REQUIRE( pn_tls_is_secure(cli_tls) == false ); // negotiation incomplete both sides
  REQUIRE( pn_tls_is_secure(srv_tls) == false );

  /* server hello part2: client side */

  REQUIRE( pn_tls_need_encrypt_output_buffers(cli_tls) == false );
  REQUIRE( pn_tls_give_decrypt_input_buffers(cli_tls, rb_array, 1) == 1 );
  REQUIRE( pn_tls_process(cli_tls) == 0 );
  REQUIRE( pn_tls_take_decrypt_input_buffers(cli_tls, rb_array, 2) == 1 );
  REQUIRE( rb_array[0].bytes == wire_bytes );

  REQUIRE( pn_tls_is_secure(cli_tls) == true );  // Protocol and certificate acceptable to client
  REQUIRE( pn_tls_is_secure(srv_tls) == false ); // Server doesn't know yet

  /* client side finish record and application data */

  REQUIRE( pn_tls_need_encrypt_output_buffers(cli_tls) == true );
  set_rbuf(&wire_buf, wire_bytes, sizeof(wire_bytes), 0);
  REQUIRE( pn_tls_give_encrypt_output_buffers(cli_tls, &wire_buf, 1) == 1 );

  size_t len = sizeof(cli_data);
  memcpy(app_bytes, cli_data, len);
  app_buf.size = len;
  REQUIRE( pn_tls_give_encrypt_input_buffers(cli_tls, &app_buf, 1) );

  REQUIRE( pn_tls_process(cli_tls) == 0 );
  REQUIRE( pn_tls_take_encrypt_input_buffers(cli_tls, rb_array, 2) == 1 );
  REQUIRE( rb_array[0].bytes == app_bytes );
  REQUIRE( pn_tls_take_encrypt_output_buffers(cli_tls, rb_array, 2) == 1 );
  REQUIRE( rb_array[0].bytes == wire_bytes );
  REQUIRE( rb_array[0].size < sizeof(wire_bytes) );  // Strictly less expected.

  /* server side */

  REQUIRE( pn_tls_give_decrypt_input_buffers(srv_tls, rb_array, 1) == 1 );
  REQUIRE( pn_tls_process(srv_tls) == 0 );
  REQUIRE( pn_tls_is_secure(srv_tls) == true ); // Handshake complete at server
  REQUIRE( pn_tls_need_decrypt_output_buffers(srv_tls) == true ); // have client app data

  memset(app_bytes, 0, sizeof(app_bytes));
  set_rbuf(&app_buf, app_bytes, sizeof(app_bytes), 0);
  REQUIRE( pn_tls_give_decrypt_output_buffers(srv_tls, &app_buf, 1) == 1 );
  REQUIRE( pn_tls_process(srv_tls) == 0 );
  REQUIRE( pn_tls_take_decrypt_output_buffers(srv_tls, rb_array, 2) == 1 );
  REQUIRE( rb_array[0].bytes == app_bytes );
  REQUIRE( rb_array[0].size == sizeof(cli_data) );
  REQUIRE( strncmp(rb_array[0].bytes, cli_data, sizeof(cli_data)) == 0 );
  REQUIRE( pn_tls_take_decrypt_input_buffers(srv_tls, rb_array, 2) == 1 );
  REQUIRE( rb_array[0].bytes == wire_bytes );

  memset(app_bytes, 0, sizeof(app_bytes));  // Received client data, send server data
  set_rbuf(&app_buf, app_bytes, sizeof(app_bytes), 0);
  len = sizeof(srv_data);
  memcpy(app_bytes, srv_data, len);
  app_buf.size = len;
  REQUIRE( pn_tls_give_encrypt_input_buffers(srv_tls, &app_buf, 1) );
  set_rbuf(&wire_buf, wire_bytes, sizeof(wire_bytes), 0);
  REQUIRE( pn_tls_give_encrypt_output_buffers(srv_tls, &wire_buf, 1) == 1 );
  pn_tls_close_output(srv_tls);  // Finished sending.
  REQUIRE( pn_tls_process(srv_tls) == 0 );
  REQUIRE( pn_tls_take_encrypt_input_buffers(srv_tls, rb_array, 2) == 1 );
  REQUIRE( rb_array[0].bytes == app_bytes );
  REQUIRE( pn_tls_take_encrypt_output_buffers(srv_tls, rb_array, 2) == 1 );
  REQUIRE( rb_array[0].bytes == wire_bytes );
  REQUIRE( rb_array[0].size < sizeof(wire_bytes) );

  /* client side: read server data and confirm end of TLS session */

  REQUIRE( pn_tls_is_input_closed(cli_tls) == false );
  memset(app_bytes, 0, sizeof(app_bytes));
  set_rbuf(&app_buf, app_bytes, sizeof(app_bytes), 0);
  REQUIRE( pn_tls_give_decrypt_output_buffers(cli_tls, &app_buf, 1) == 1 );
  REQUIRE( pn_tls_give_decrypt_input_buffers(cli_tls, rb_array, 1) == 1 );
  REQUIRE( pn_tls_process(cli_tls) == 0 );
  REQUIRE( pn_tls_take_decrypt_input_buffers(cli_tls, rb_array, 2) == 1 );
  REQUIRE( rb_array[0].bytes == wire_bytes );
  REQUIRE( pn_tls_take_decrypt_output_buffers(cli_tls, rb_array, 2) == 1 );
  REQUIRE( rb_array[0].bytes == app_bytes );
  REQUIRE( rb_array[0].size == sizeof(srv_data) );
  REQUIRE( strncmp(rb_array[0].bytes, srv_data, sizeof(srv_data)) == 0 );

  REQUIRE( pn_tls_is_input_closed(cli_tls) == true );
  set_rbuf(&wire_buf, wire_bytes, sizeof(wire_bytes), 0);
  REQUIRE( pn_tls_give_encrypt_output_buffers(cli_tls, &wire_buf, 1) == 1 );
  pn_tls_close_output(cli_tls);  // Initiate symetric closure record
  REQUIRE( pn_tls_process(cli_tls) == 0 );
  REQUIRE( pn_tls_take_encrypt_output_buffers(cli_tls, rb_array, 2) == 1 );
  REQUIRE( rb_array[0].bytes == wire_bytes );
  REQUIRE( rb_array[0].size < sizeof(wire_bytes) );

  /* server side */

  REQUIRE( pn_tls_is_input_closed(srv_tls) == false );
  REQUIRE( pn_tls_give_decrypt_input_buffers(srv_tls, rb_array, 1) == 1 );
  REQUIRE( pn_tls_process(srv_tls) == 0 );
  REQUIRE( pn_tls_take_decrypt_input_buffers(srv_tls, rb_array, 2) == 1 );
  REQUIRE( pn_tls_is_input_closed(srv_tls) == true );

  /* clean up */

  pn_tls_stop(cli_tls);
  REQUIRE( pn_tls_take_encrypt_input_buffers(cli_tls, rb_array, 1) == 0 );
  REQUIRE( pn_tls_take_encrypt_output_buffers(cli_tls, rb_array, 1) == 0 );
  REQUIRE( pn_tls_take_decrypt_input_buffers(cli_tls, rb_array, 1) == 0 );
  REQUIRE( pn_tls_take_decrypt_output_buffers(cli_tls, rb_array, 1) == 0 );
  pn_tls_free(cli_tls);

  pn_tls_stop(srv_tls);
  REQUIRE( pn_tls_take_encrypt_input_buffers(srv_tls, rb_array, 1) == 0 );
  REQUIRE( pn_tls_take_encrypt_output_buffers(srv_tls, rb_array, 1) == 0 );
  REQUIRE( pn_tls_take_decrypt_input_buffers(srv_tls, rb_array, 1) == 0 );
  REQUIRE( pn_tls_take_decrypt_output_buffers(srv_tls, rb_array, 1) == 0 );
  pn_tls_free(srv_tls);

  pn_tls_config_free(client_config);
  pn_tls_config_free(server_config);
}

pn_tls_config_t * default_client_config(void) {
  pn_tls_config_t *cfg = pn_tls_config(PN_TLS_MODE_CLIENT);
  if (cfg && pn_tls_config_set_trusted_certs(cfg, CERTIFICATE("tserver")) == 0)
    return cfg;
  pn_tls_config_free(cfg);
  return NULL;
}

pn_tls_config_t *default_server_config(void) {
  pn_tls_config_t *cfg = pn_tls_config(PN_TLS_MODE_SERVER);
  if (cfg && SET_CREDENTIALS(cfg, "tserver") == 0)
    return cfg;
  pn_tls_config_free(cfg);
  return NULL;
}

pn_raw_buffer_t new_rbuf(size_t sz) {
  pn_raw_buffer_t rb = {0};
  rb.bytes = (char *) malloc(sz);
  // REQUIRE(rb.bytes != NULL);
  rb.capacity = sz;
  return rb;
}

void free_rbuf(pn_raw_buffer_t &rb) {
  free(rb.bytes);
  rb = {0};
}

bool is_null(pn_raw_buffer_t &rb) {
  return rb.bytes == NULL && rb.capacity == 0 && rb.size == 0 && rb.offset == 0;
}

bool is_valid(pn_raw_buffer_t &rb) {
  return rb.bytes && rb.capacity && (rb.size + rb.offset <= rb.capacity);
}

void drain_processed_input_bufs(pn_tls_t *tls) {
  pn_raw_buffer_t rb;
  while (pn_tls_take_encrypt_input_buffers(tls, &rb, 1) == 1)
    free_rbuf(rb);
  while (pn_tls_take_decrypt_input_buffers(tls, &rb, 1) == 1)
    free_rbuf(rb);
}

void drain_processed_output_bufs(pn_tls_t *tls) {
  pn_raw_buffer_t rb;
  while (pn_tls_take_encrypt_output_buffers(tls, &rb, 1) == 1)
    free_rbuf(rb);
  while (pn_tls_take_decrypt_output_buffers(tls, &rb, 1) == 1)
    free_rbuf(rb);
}

// Call after a single pump.  Data will be flushed if an output buffer is staged.
bool has_encrypted_data(pn_tls_t *tls) {
  return pn_tls_get_last_encrypt_output_buffer_size(tls) ||
    pn_tls_need_encrypt_output_buffers(tls);
}

bool has_encrypt_room(pn_tls_t *tls) {
  return pn_tls_get_encrypt_input_buffer_capacity(tls) > 1;
}

bool has_decrypt_room(pn_tls_t *tls) {
  return pn_tls_get_decrypt_input_buffer_capacity(tls) > 1;
}

struct TestPeer {
  bool isServer;
  pn_tls_config_t *config;
  pn_tls_t *tls;
  TestPeer(bool is_server) : isServer(is_server), config(NULL), tls(NULL) {}
  ~TestPeer() {
    if (tls) {
      cleanup();
      pn_tls_free(tls);
    }
    if (config) pn_tls_config_free(config);
  }
  void init() {
    bool dflt = false;
    if (!config) {
      config = isServer ? default_server_config() : default_client_config();
      dflt = true;
    }
    REQUIRE(config);
    if (!tls) tls = pn_tls(config);
    REQUIRE(tls);
    if (dflt && !isServer)
      pn_tls_set_peer_hostname(tls, "test_server");
    REQUIRE(pn_tls_start(tls) == 0);
  }
  void cleanup() {
    if (tls) {
      pn_tls_stop(tls);
      drain_processed_input_bufs(tls);
      drain_processed_output_bufs(tls);
    }
  }
};

// Transfer one raw buffer of encrypted data from each peer to the other.  Test programs
// can call this at any time so number of processed/unprocessed input/output buffers is
// unknown.
void pump(TestPeer &a, TestPeer &b) {
  pn_raw_buffer_t from_a = {0};
  pn_raw_buffer_t from_b = {0};
  pn_tls_process(a.tls);
  pn_tls_process(b.tls);

  drain_processed_input_bufs(b.tls);
  if (has_decrypt_room(b.tls)) {
    if (pn_tls_take_encrypt_output_buffers(a.tls, &from_a, 1) == 1) {
      REQUIRE(is_valid(from_a));
    } else {
      REQUIRE(is_null(from_a));
      if (pn_tls_need_encrypt_output_buffers(a.tls)) {
        pn_raw_buffer_t rbuf = new_rbuf(65536);
        REQUIRE(pn_tls_give_encrypt_output_buffers(a.tls, &rbuf, 1) == 1);
        REQUIRE(pn_tls_process(a.tls) == 0);
        REQUIRE(pn_tls_take_encrypt_output_buffers(a.tls, &from_a, 1) == 1);
        REQUIRE(is_valid(from_a));
      }
    }
  }

  // Symetrically opposite.
  drain_processed_input_bufs(a.tls);
  if (has_decrypt_room(a.tls)) {
    if (pn_tls_take_encrypt_output_buffers(b.tls, &from_b, 1) == 1) {
      REQUIRE(is_valid(from_b));
    } else {
      REQUIRE(is_null(from_b));
      if (pn_tls_need_encrypt_output_buffers(b.tls)) {
        pn_raw_buffer_t rbuf = new_rbuf(65536);
        REQUIRE(pn_tls_give_encrypt_output_buffers(b.tls, &rbuf, 1) == 1);
        pn_tls_process(b.tls);
        REQUIRE(pn_tls_take_encrypt_output_buffers(b.tls, &from_b, 1) == 1);
        REQUIRE(is_valid(from_b));
      }
    }
  }

  // Now allow each peer see and act on the other's transferred data.
  if (!is_null(from_a)) {
    REQUIRE(pn_tls_give_decrypt_input_buffers(b.tls, &from_a, 1) == 1);
    pn_tls_process(b.tls);
  }
  if (!is_null(from_b)) {
    REQUIRE(pn_tls_give_decrypt_input_buffers(a.tls, &from_b, 1) == 1);
    pn_tls_process(a.tls);
  }
}

struct TestPair {
  TestPeer client;
  TestPeer server;

  TestPair() : client(false), server(true) {}
  void init() {
    client.init();
    server.init();
  }
  void singlePump() {
    pump(client, server);
  }
  bool canPump() {
    if (has_encrypted_data(client.tls) && has_encrypt_room(server.tls))
      return true;
    if (has_encrypted_data(server.tls) && has_encrypt_room(client.tls))
      return true;
    // No data or no place to put it.
    return false;
  }
  void multiPump() {
    do {
      singlePump();
    } while (canPump());
  }
};

TEST_CASE("default - no data") {
  TestPair tp;
  tp.init();  // just use default setup
  tp.multiPump();
  REQUIRE(pn_tls_is_secure(tp.client.tls));
  REQUIRE(pn_tls_is_secure(tp.server.tls));
  // Handshake OK.  Close session.
  pn_tls_close_output(tp.client.tls);
  pn_tls_close_output(tp.server.tls);
  tp.multiPump();
  REQUIRE(pn_tls_is_input_closed(tp.client.tls));
  REQUIRE(pn_tls_get_session_error(tp.client.tls) == 0);
  REQUIRE(pn_tls_is_input_closed(tp.server.tls));
  REQUIRE(pn_tls_get_session_error(tp.server.tls) == 0);
}

TEST_CASE("missing client cert") {
  TestPair tp;
  tp.server.config = default_server_config();
  REQUIRE(pn_tls_config_set_peer_authentication(tp.server.config, PN_TLS_VERIFY_PEER, CERTIFICATE("tclient")) == 0);
  tp.init();
  const char *early_data = "early data";
  size_t len = strlen(early_data);
  pn_raw_buffer_t early_data_buf = new_rbuf(1024);
  memcpy(early_data_buf.bytes, early_data, len);
  early_data_buf.size = len;
  REQUIRE(pn_tls_give_encrypt_input_buffers(tp.client.tls, &early_data_buf, 1) == 1);

  tp.singlePump(); // Client hello
  tp.singlePump(); // Server hello and request for cert
  tp.singlePump(); // Client finish (and early data if TLS1.3).  No client cert configured or provided.

  REQUIRE(pn_tls_need_decrypt_output_buffers(tp.server.tls) == false); // PROTON-2535
  REQUIRE(pn_tls_get_session_error(tp.server.tls) != 0);
  REQUIRE(pn_tls_get_session_error(tp.client.tls) == 0);

  tp.singlePump(); // Server error alert
  REQUIRE(pn_tls_get_session_error(tp.client.tls) != 0);

  char ebuf[4096];
  memset(ebuf, 0, sizeof(ebuf));
  len = pn_tls_get_session_error_string(tp.server.tls, ebuf, sizeof(ebuf));
  REQUIRE(len != 0);
  REQUIRE(strstr(ebuf, "peer did not return a certificate"));

  memset(ebuf, 0, sizeof(ebuf));
  len = pn_tls_get_session_error_string(tp.client.tls, ebuf, sizeof(ebuf));
  REQUIRE(len != 0);
  REQUIRE(strstr(ebuf, "certificate required"));

  set_rbuf(&early_data_buf, NULL, 0, 0);
  reset_rbuf(&early_data_buf);
}
