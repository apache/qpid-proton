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

#include "./pn_test_proactor.hpp"

#include <proton/connection.h>
#include <proton/condition.h>
#include <proton/delivery.h>
#include <proton/link.h>
#include <proton/listener.h>
#include <proton/netaddr.h>
#include <proton/proactor.h>
#include <proton/session.h>
#include <proton/sasl.h>
#include <proton/ssl.h>
#include <proton/transport.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct app_data_t {
  const char *amqp_address;
  const char *container_id;

  pn_ssl_domain_t *server_ssl_domain;

  bool connection_succeeded;
  bool transport_error;
} app_data_t;

/* Note must be run in the current directory to find certificate files */
#define SSL_FILE(NAME) "ssl-certs/" NAME
#define SSL_PW(NAME) NAME "pw"
/* Windows vs. OpenSSL certificates */
#if defined(_WIN32)
#  define CERTIFICATE(NAME) SSL_FILE(NAME "-certificate.p12")
#  define SET_CREDENTIALS(DOMAIN, NAME)                                 \
  pn_ssl_domain_set_credentials(DOMAIN, SSL_FILE(NAME "-full.p12"), "", SSL_PW(NAME))
#else
#  define CERTIFICATE(NAME) SSL_FILE(NAME "-certificate.pem")
#  define SET_CREDENTIALS(DOMAIN, NAME)                                 \
  pn_ssl_domain_set_credentials(DOMAIN, CERTIFICATE(NAME), SSL_FILE(NAME "-private-key.pem"), SSL_PW(NAME))
#endif


/* Returns true to continue, false if finished */
static bool server_handler(app_data_t* app, pn_event_t* event) {
  pn_listener_t *l = pn_event_listener(event);
  switch (pn_event_type(event)) {

   // Server side
   case PN_LISTENER_ACCEPT: {
     /* Configure a transport to allow SSL and SASL connections. See ssl_domain setup in main() */
     pn_transport_t *t = pn_transport();
     pn_transport_set_server(t); /* Must call before pn_sasl() */
     pn_sasl_allowed_mechs(pn_sasl(t), "ANONYMOUS");
     pn_ssl_init(pn_ssl(t), app->server_ssl_domain, NULL);
     pn_listener_accept2(l, NULL, t);

     /* Accept only one connection */
     pn_listener_close(l);
     break;
   }

   case PN_TRANSPORT_CLOSED:
    break;

   default: break;
  }
  return true;
}

static bool client_handler(app_data_t* app, pn_event_t* event) {
  switch (pn_event_type(event)) {
   // Client side
   case PN_CONNECTION_INIT: {
     pn_connection_t* c = pn_event_connection(event);
     pn_session_t* s = pn_session(pn_event_connection(event));
     pn_connection_set_container(c, app->container_id);
     pn_connection_open(c);
     pn_session_open(s);
     {
     pn_link_t* l = pn_sender(s, "my_sender");
     pn_terminus_set_address(pn_link_target(l), app->amqp_address);
     pn_link_open(l);
     break;
     }
   }

   case PN_CONNECTION_BOUND: {
     break;
   }

   case PN_CONNECTION_REMOTE_OPEN:
    app->connection_succeeded = true;
    pn_connection_close(pn_event_connection(event));
    break;

   case PN_TRANSPORT_ERROR:
    app->transport_error = true;
    break;

   case PN_CONNECTION_REMOTE_CLOSE:
    pn_connection_close(pn_event_connection(event));
    break;

   case PN_SESSION_REMOTE_CLOSE:
    pn_connection_close(pn_event_connection(event));
    break;

   case PN_LINK_REMOTE_CLOSE:
   case PN_LINK_REMOTE_DETACH:
    pn_connection_close(pn_event_connection(event));
    break;

   default: break;
  }
  return true;
}

typedef bool handler_t(app_data_t* app, pn_event_t* event);
void run(pn_proactor_t *p, app_data_t *app, handler_t *shandler, handler_t *chandler) {
  /* Loop and handle server/client events */
  do {
    pn_event_batch_t *events = pn_proactor_wait(p);
    pn_event_t *e;
    for (e = pn_event_batch_next(events); e; e = pn_event_batch_next(events)) {
      if (pn_event_type(e)==PN_PROACTOR_INACTIVE) {
        return;
      }

      if (pn_event_listener(e)) {
        if (!shandler(app, e)) {
          return;
        }
      } else {
        if (!chandler(app, e)) {
          return;
        }
      }
    }
    pn_proactor_done(p, events);
  } while(true);
}

static void run_until_listener_open(pn_proactor_t* p) {
  do {
    pn_event_batch_t *events = pn_proactor_wait(p);
    pn_event_t *e;
    for (e = pn_event_batch_next(events); e; e = pn_event_batch_next(events)) {
      if (pn_event_type(e)==PN_LISTENER_OPEN) {
        pn_proactor_done(p, events);
        return;
      }
    }
    pn_proactor_done(p, events);
  } while (true);
}

static void setup_connection(pn_proactor_t* proactor, pn_transport_t* t) {
  pn_listener_t* l = pn_listener();
  pn_proactor_listen(proactor, l, ":0", 16);
  // Don't know port until the listener is open
  run_until_listener_open(proactor);

  // Construct connect address (from listener port)
  const pn_netaddr_t* a = pn_listener_addr(l);
  char port[32];
  port[0] = ':';
  pn_netaddr_host_port(a, NULL, 0, port+1, 31);

  INFO("Connecting to " << port);
  pn_proactor_connect2(proactor, NULL, t, port);
}

TEST_CASE("ssl certificate verification tests") {
  struct app_data_t app = {0};

  app.container_id = "ssl-test";
  app.amqp_address = "fubar";

  pn_test::auto_free<pn_proactor_t, pn_proactor_free> proactor(pn_proactor());

  /* Configure server for default SSL */
  pn_test::auto_free<pn_ssl_domain_t, pn_ssl_domain_free>
    sd(pn_ssl_domain(PN_SSL_MODE_SERVER));
  app.server_ssl_domain = sd;

  /* Configure a client for SSL */
  pn_transport_t *t = pn_transport();
  pn_test::auto_free<pn_ssl_domain_t, pn_ssl_domain_free>
    cd(pn_ssl_domain(PN_SSL_MODE_CLIENT));

  SECTION("Default connections don't verify to self signed server (even with correct name)") {
    REQUIRE(SET_CREDENTIALS(sd, "tserver") == 0);
    REQUIRE(pn_ssl_init(pn_ssl(t), NULL, NULL) == 0);
    REQUIRE(pn_ssl_set_peer_hostname(pn_ssl(t), "test_server") == 0);

    setup_connection(proactor, t);

    run(proactor, &app, server_handler, client_handler);
    CHECK(app.connection_succeeded==false);
    CHECK(app.transport_error==true);
  }

  SECTION("Connections noname verify to self signed cert if cert allowed (even with no name)") {
    REQUIRE(SET_CREDENTIALS(sd, "tserver") == 0);
    REQUIRE(pn_ssl_domain_set_trusted_ca_db(cd, CERTIFICATE("tserver")) == 0);
    REQUIRE(pn_ssl_domain_set_peer_authentication(cd, PN_SSL_VERIFY_PEER, NULL) == 0);
    REQUIRE(pn_ssl_init(pn_ssl(t), cd, NULL) == 0);

    setup_connection(proactor, t);

    run(proactor, &app, server_handler, client_handler);
    CHECK(app.connection_succeeded==true);
    CHECK(app.transport_error==false);
  }

  SECTION("Connections don't noname verify to self signed cert without cert allowed") {
    REQUIRE(SET_CREDENTIALS(sd, "tserver") == 0);
    REQUIRE(pn_ssl_domain_set_peer_authentication(cd, PN_SSL_VERIFY_PEER, NULL) == 0);
    REQUIRE(pn_ssl_init(pn_ssl(t), cd, NULL) == 0);

    setup_connection(proactor, t);

    run(proactor, &app, server_handler, client_handler);
    CHECK(app.connection_succeeded==false);
    CHECK(app.transport_error==true);
  }

  SECTION("Connections verify with self signed server if cert allowed") {
    REQUIRE(SET_CREDENTIALS(sd, "tserver") == 0);
    REQUIRE(pn_ssl_domain_set_trusted_ca_db(cd, CERTIFICATE("tserver")) == 0);
    REQUIRE(pn_ssl_domain_set_peer_authentication(cd, PN_SSL_VERIFY_PEER_NAME, NULL) == 0);
    REQUIRE(pn_ssl_init(pn_ssl(t), cd, NULL) == 0);
    REQUIRE(pn_ssl_set_peer_hostname(pn_ssl(t), "test_server") == 0);

    setup_connection(proactor, t);

    run(proactor, &app, server_handler, client_handler);
    CHECK(app.connection_succeeded==true);
    CHECK(app.transport_error==false);
  }

  SECTION("Anonymous server connections don't verify") {
    REQUIRE(pn_ssl_domain_set_trusted_ca_db(cd, CERTIFICATE("tserver")) == 0);
    REQUIRE(pn_ssl_domain_set_peer_authentication(cd, PN_SSL_VERIFY_PEER_NAME, NULL) == 0);
    REQUIRE(pn_ssl_init(pn_ssl(t), cd, NULL) == 0);

    setup_connection(proactor, t);

    run(proactor, &app, server_handler, client_handler);
    CHECK(app.connection_succeeded==false);
    CHECK(app.transport_error==true);
  }

  SECTION("Anonymous connections connect if anonymous allowed") {
#ifndef _WIN32
    REQUIRE(pn_ssl_domain_set_peer_authentication(cd, PN_SSL_ANONYMOUS_PEER, NULL) == 0);
    REQUIRE(pn_ssl_init(pn_ssl(t), cd, NULL) == 0);

    setup_connection(proactor, t);

    run(proactor, &app, server_handler, client_handler);
    CHECK(app.connection_succeeded==true);
    CHECK(app.transport_error==false);
#else
    SUCCEED("Skipped: Windows schannel does not support anonymous connections");
#endif
  }

  SECTION("Default server (anonymous) doesn't verify against default client (verify peername)") {
    app.server_ssl_domain = 0;
    REQUIRE(pn_ssl_init(pn_ssl(t), NULL, NULL) == 0);

    setup_connection(proactor, t);

    run(proactor, &app, server_handler, client_handler);
    CHECK(app.connection_succeeded==false);
    CHECK(app.transport_error==true);
  }

  SECTION("Default server (anonymous) connects if anonymous allowed") {
#ifndef _WIN32
    app.server_ssl_domain = 0;
    REQUIRE(pn_ssl_domain_set_peer_authentication(cd, PN_SSL_ANONYMOUS_PEER, NULL) == 0);
    REQUIRE(pn_ssl_init(pn_ssl(t), cd, NULL) == 0);

    setup_connection(proactor, t);

    run(proactor, &app, server_handler, client_handler);
    CHECK(app.connection_succeeded==true);
    CHECK(app.transport_error==false);
#else
    SUCCEED("Skipped: Windows schannel does not support anonymous connections");
#endif
  }
}
