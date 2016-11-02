#ifndef PROTON_PROACTOR_H
#define PROTON_PROACTOR_H

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

#include <proton/types.h>
#include <proton/import_export.h>
#include <proton/listener.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pn_condition_t pn_condition_t;

/**
 * @file
 *
 * **Experimental**: Proactor API for the the proton @ref engine.
 *
 * @defgroup proactor Proactor
 *
 * **Experimental**: Proactor API for portable, multi-threaded, asynchronous applications.
 *
 * The proactor establishes and listens for connections. It creates the @ref
 * "transport" transport that sends and receives data over the network and
 * delivers @ref "events" event to application threads for processing.
 *
 * ## Multi-threading
 *
 * The @ref proactor is thread-safe, but the @ref "protocol engine" is not.  The
 * proactor ensures that each @ref "connection" connection and its associated
 * values (@ref session, @ref link etc.) is processed sequentially, even if there
 * are multiple application threads. See pn_proactor_wait()
 *
 * @{
 */

/**
 * The proactor creates and manage @ref "transports" transport and delivers @ref
 * "event" events to the application.
 *
 */
typedef struct pn_proactor_t pn_proactor_t;

/**
 * Create a proactor. Must be freed with pn_proactor_free()
 */
pn_proactor_t *pn_proactor(void);

/**
 * Free the proactor.
 */
void pn_proactor_free(pn_proactor_t*);

/* FIXME aconway 2016-11-12: connect and listen need options to enable
   things like websockets, alternate encryption or other features.
   The "extra" parameter will be replaced by an "options" parameter
   that will include providing extra data and other manipulators
   to affect how the connection is processed.
*/

/**
 * Asynchronous connect: a connection and transport will be created, the
 * relevant events will be returned by pn_proactor_wait()
 *
 * Errors are indicated by PN_TRANSPORT_ERROR/PN_TRANSPORT_CLOSE events.
 *
 * @param extra bytes to copy to pn_connection_get_extra() on the new connection, @ref
 * pn_rwbytes_null for nothing.
 *
 * @return error if the connect cannot be initiated e.g. an allocation failure.
 * IO errors will be returned as transport events via pn_proactor_wait()
 */
int pn_proactor_connect(pn_proactor_t*, const char *host, const char *port, pn_bytes_t extra);

/**
 * Asynchronous listen: start listening, connections will be returned by pn_proactor_wait()
 * An error are indicated by PN_LISTENER_ERROR event.
 *
 * @param extra bytes to copy to pn_connection_get_extra() on the new connection, @ref
 * pn_rwbytes_null for nothing.
 *
 * @return error if the connect cannot be initiated e.g. an allocation failure.
 * IO errors will be returned as transport events via pn_proactor_wait()
 */
pn_listener_t *pn_proactor_listen(pn_proactor_t *, const char *host, const char *port, int backlog, pn_bytes_t extra);

/**
 * Wait for an event. Can be called in multiple threads concurrently.
 * You must call pn_event_done() when the event has been handled.
 *
 * The proactor ensures that events that cannot be handled concurrently
 * (e.g. events for for the same connection) are never returned concurrently.
 */
pn_event_t *pn_proactor_wait(pn_proactor_t* d);

/**
 * Cause PN_PROACTOR_INTERRUPT to be returned to exactly one thread calling wait()
 * for each call to pn_proactor_interrupt(). Thread safe.
 */
void pn_proactor_interrupt(pn_proactor_t* d);

/**
 * Cause PN_PROACTOR_TIMEOUT to be returned to a thread calling wait() after
 * timeout milliseconds. Thread safe.
 *
 * Note calling pn_proactor_set_timeout() again before the PN_PROACTOR_TIMEOUT is
 * delivered will cancel the previous timeout and deliver an event only after
 * the new timeout.
 */
void pn_proactor_set_timeout(pn_proactor_t* d, pn_millis_t timeout);

/**
 * Cause a PN_CONNECTION_WAKE event to be returned by the proactor, even if
 * there are no IO events pending for the connection.
 *
 * Thread safe: this is the only pn_connection_ function that can be
 * called concurrently.
 *
 * Wakes can be "coalesced" - if several pn_connection_wake() calls happen
 * concurrently, there may be only one PN_CONNECTION_WAKE event.
 */
void pn_connection_wake(pn_connection_t *c);

/**
 * The proactor that created the connection.
 */
pn_proactor_t *pn_connection_proactor(pn_connection_t *c);

/**
 * Call when a proactor event has been handled. Does nothing if not a proactor event.
 *
 * Thread safe: May be called from any thread but must be called exactly once
 * for each event returned by pn_proactor_wait()
 */
void pn_event_done(pn_event_t *);

/**
 * Get the proactor that created the event or NULL.
 */
pn_proactor_t *pn_event_proactor(pn_event_t *);

/**
 * Get the listener for the event or NULL.
 */
pn_listener_t *pn_event_listener(pn_event_t *);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif // PROTON_PROACTOR_H
