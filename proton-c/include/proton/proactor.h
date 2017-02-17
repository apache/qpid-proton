#ifndef PROTON_PROACTOR_H
#define PROTON_PROACTOR_H 1

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

#include <proton/event.h>
#include <proton/import_export.h>
#include <proton/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 *
 * **Experimental** - Multithreaded IO
 *
 * The proactor associates a @ref connection with a @ref transport,
 * either by making an outgoing connection or accepting an incoming
 * one.  It delivers @ref event "events" to application threads for
 * handling.
 *
 * ## Multi-threading
 *
 * The @ref proactor is thread-safe, but the protocol engine is not.
 * The proactor ensures that each @ref connection and its associated
 * values (@ref session, @ref link etc.) is handle sequentially, even
 * if there are multiple application threads. See pn_proactor_wait().
 *
 * @addtogroup proactor
 * @{
 */

/**
 * Create a proactor. Must be freed with pn_proactor_free()
 */
PNP_EXTERN pn_proactor_t *pn_proactor(void);

/**
 * Free the proactor. Abort any open network connections and clean up all
 * associated resources.
 */
PNP_EXTERN void pn_proactor_free(pn_proactor_t *proactor);

/**
 * Connect connection to host/port. Connection and transport events will be
 * returned by pn_proactor_wait()
 *
 * @param[in] proactor the proactor object
 * @param[in] connection the proactor takes ownership, do not free
 * @param[in] host address to connect on
 * @param[in] port port to connect to
 *
 * @return error on immediate error, e.g. an allocation failure.
 * Other errors are indicated by connection or transport events via
PNP_EXTERN  * pn_proactor_wait()
 */
PNP_EXTERN int pn_proactor_connect(pn_proactor_t *proactor, pn_connection_t *connection,
                        const char *host, const char *port);

/**
 * Start listening with listener.  pn_proactor_wait() will return a
 * PN_LISTENER_ACCEPT event when a connection can be accepted.
 *
 * @param[in] proactor the proactor object
 * @param[in] listener proactor takes ownership of listener, do not free
 * @param[in] host address to listen on
 * @param[in] port port to listen on
 * @param[in] backlog number of connection requests to queue
 *
 * @return error on immediate error, e.g. an allocation failure.
 * Other errors are indicated by pn_listener_condition() on the
 * PN_LISTENER_CLOSE event.
 */
PNP_EXTERN int pn_proactor_listen(pn_proactor_t *proactor, pn_listener_t *listener,
                       const char *host, const char *port, int backlog);

/**
 * Wait until there is at least one event to handle.
 * Always returns a non-empty batch of events.
 *
 * You must call pn_proactor_done() when you are finished with the batch, you
 * must not use the batch pointer after calling pn_proactor_done().
 *
 * Normally it is most efficient to handle the entire batch in one thread, but
 * you can call pn_proactor_done() on an unfinished the batch. The remaining
 * events will be returned by another call to pn_proactor_done(), possibly in a
 * different thread.
 *
 * @note You can generate events to force threads to wake up from
 * pn_proactor_wait() using pn_proactor_interrupt(), pn_proactor_set_timeout()
 * and pn_connection_wake()
 *
 * @note Thread-safe: can be called concurrently. Events in a single
 * batch must be handled in sequence, but batches returned by separate
 * calls can be handled concurrently.
 */
PNP_EXTERN pn_event_batch_t *pn_proactor_wait(pn_proactor_t *proactor);

/**
 * Return a batch of events if one is available immediately, otherwise return NULL.  If it
 * does return an event batch, the rules are the same as for pn_proactor_wait()
 */
PNP_EXTERN pn_event_batch_t *pn_proactor_get(pn_proactor_t *proactor);

/**
 * Call when done handling a batch of events.
 *
 * Must be called exactly once to match each call to
 * pn_proactor_wait().
 *
 * @note Thread-safe: may be called from any thread provided the
 * exactly once rule is respected.
 */
PNP_EXTERN void pn_proactor_done(pn_proactor_t *proactor, pn_event_batch_t *events);

/**
 * Cause PN_PROACTOR_INTERRUPT to be returned to exactly one call of
 * pn_proactor_wait().
 *
 * If threads are blocked in pn_proactor_wait(), one of them will be
 * interrupted, otherwise the interrupt will be returned by a future
 * call to pn_proactor_wait(). Calling pn_proactor_interrupt() N times
 * will return PN_PROACTOR_INTERRUPT to N current or future calls of
 * pn_proactor_wait()
 *
 * @note Thread-safe.
 */
PNP_EXTERN void pn_proactor_interrupt(pn_proactor_t *proactor);

/**
 * Cause PN_PROACTOR_TIMEOUT to be returned to a thread calling wait()
 * after timeout milliseconds. Thread-safe.
 *
 * Note that calling pn_proactor_set_timeout() again before the
 * PN_PROACTOR_TIMEOUT is delivered will cancel the previous timeout
 * and deliver an event only after the new
 * timeout. `pn_proactor_set_timeout(0)` will cancel the timeout
 * without setting a new one.
 */
PNP_EXTERN void pn_proactor_set_timeout(pn_proactor_t *proactor, pn_millis_t timeout);

/**
 * Cause a PN_CONNECTION_WAKE event to be returned by the proactor, even if
 * there are no IO events pending for the connection.
 *
 * @note Thread-safe: this is the only pn_connection_ function that
 * can be called concurrently.
 *
 * Wakes can be "coalesced" - if several pn_connection_wake() calls happen
 * concurrently, there may be only one PN_CONNECTION_WAKE event.
 */
PNP_EXTERN void pn_connection_wake(pn_connection_t *connection);

/**
 * Return the proactor associated with a connection or NULL.
 */
PNP_EXTERN pn_proactor_t *pn_connection_proactor(pn_connection_t *connection);

/**
 * Return the proactor associated with an event or NULL.
 */
PNP_EXTERN pn_proactor_t *pn_event_proactor(pn_event_t *event);

/**
 * Return the listener associated with an event or NULL.
 */
PNP_EXTERN pn_listener_t *pn_event_listener(pn_event_t *event);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* proactor.h */
