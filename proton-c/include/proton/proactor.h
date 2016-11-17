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
 * **Experimental**: Proactor API for the the proton @ref engine
 *
 * @defgroup proactor Proactor
 *
 * **Experimental**: Proactor API for portable, multi-threaded, asynchronous applications.
 *
 * The proactor establishes and listens for connections. It creates
 * the @ref transport that sends and receives data over the network and
 * delivers @ref event to application threads for handling.
 *
 * **Multi-threading**:
 * The @ref proactor is thread-safe, but the @ref engine is not.  The proactor
 * ensures that each @ref connection and its associated values (@ref session,
 * @ref link etc.) is handle sequentially, even if there are multiple
 * application threads. See pn_proactor_wait()
 *
 * @{
 */

/**
 * The proactor.
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
 * Wait for events to handle. Call pn_proactor_done() after handling events.
 *
 * Thread safe: pn_proactor_wait() can be called concurrently, but the events in
 * the returned ::pn_event_batch_t must be handled sequentially.
 *
 * The proactor always returns events that must be handled sequentially in the
 * same batch or sequentially in a later batch after pn_proactor_done(). Any
 * events returned concurrently by pn_proactor_wait() are safe to handle
 * concurrently.
 */
pn_event_batch_t *pn_proactor_wait(pn_proactor_t* d);

/**
 * Call when done handling events.
 *
 * It is generally most efficient to handle the entire batch in the thread
 * that calls pn_proactor_wait(), then call pn_proactor_done(). If you call
 * pn_proactor_done() earlier, the remaining events will be returned again by
 * pn_proactor_wait(), possibly to another thread.
 */
void pn_proactor_done(pn_proactor_t* d, pn_event_batch_t *events);

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
