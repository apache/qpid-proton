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
 * The proactor associates a @ref connection with a @ref transport, either
 * by making an outgoing connection or accepting an incoming one.
 * It delivers @ref event "events" to application threads for handling.
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
 * The proactor, see pn_proactor()
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
 * Connect connection to host/port. Connection and transport events will be
 * returned by pn_proactor_wait()
 *
 * @param[in] connection the proactor takes ownership, do not free
 * @param[in] host address to connect on
 * @param[in] port port to connect to
 *
 * @return error on immediate error, e.g. an allocation failure.
 * Other errors are indicated by connection or transport events via pn_proactor_wait()
 */
int pn_proactor_connect(pn_proactor_t*, pn_connection_t *connection, const char *host, const char *port);

/**
 * Start listening with listener.
 * pn_proactor_wait() will return a PN_LISTENER_ACCEPT event when a connection can be accepted.
 *
 * @param[in] listener proactor takes ownership of listener, do not free
 * @param[in] host address to listen on
 * @param[in] port port to listen on
 * @param[in] backlog number of connection requests to queue
 *
 * @return error on immediate error, e.g. an allocation failure.
 * Other errors are indicated by pn_listener_condition() on the PN_LISTENER_CLOSE event.
 */
int pn_proactor_listen(pn_proactor_t *, pn_listener_t *listener, const char *host, const char *port, int backlog);

/**
 * Wait for events to handle.
 *
 * Handle events in the returned batch by calling pn_event_batch_next() until it
 * returns NULL. You must call pn_proactor_done() to when you are finished.
 *
 * If you call pn_proactor_done() before finishing the batch, the remaining
 * events will be returned again by another call pn_proactor_wait().  This is
 * less efficient, but allows you to handle part of a batch and then hand off
 * the rest to another thread.
 *
 * Thread safe: can be called concurrently. Events in a single batch must be
 * handled in sequence, but batches returned by separate calls to
 * pn_proactor_wait() can be handled concurrently.
 */
pn_event_batch_t *pn_proactor_wait(pn_proactor_t* d);

/**
 * Call when done handling events.
 *
 * Must be called exactly once to match each call to pn_proactor_wait().
 *
 * Thread safe: may be called from any thread provided the exactly once rules is
 * respected.
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
 * Return the proactor associated with a connection or null
 */
pn_proactor_t *pn_connection_proactor(pn_connection_t *c);

/**
 * Return the proactor associated with an event or NULL.
 */
pn_proactor_t *pn_event_proactor(pn_event_t *);

/**
 * Return the listener associated with an event or NULL.
 */
pn_listener_t *pn_event_listener(pn_event_t *);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif // PROTON_PROACTOR_H
