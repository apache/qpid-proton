#ifndef PROTON_LISTENER_H
#define PROTON_LISTENER_H 1

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

#include <proton/import_export.h>
#include <proton/types.h>
#include <proton/event.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 *
 * @copybrief listener
 *
 * @addtogroup listener
 * @{
 *
 * @note Thread safety: Listener has the same thread-safety rules as a
 * @ref core object.  Calls to a single listener must be serialized
 * with the exception of pn_listener_close().
 */

/**
 * Create a listener to pass to pn_proactor_listen()
 *
 * You can use pn_listener_attachments() to set application data that can be
 * accessed when accepting connections.
 */
PNP_EXTERN pn_listener_t *pn_listener(void);

/**
 * Free a listener. You don't need to call this unless you create a listener
 * with pn_listen() but never pass it to pn_proactor_listen()
 */
PNP_EXTERN void pn_listener_free(pn_listener_t *l);

/**
 * Accept an incoming connection request using @p transport and @p connection,
 * which can be configured before the call.
 *
 * Call after a @ref PN_LISTENER_ACCEPT event.
 *
 * Errors are returned as @ref PN_TRANSPORT_CLOSED events by pn_proactor_wait().
 *
 * @note If you provide a transport, pn_listener_accept2() will call
 * pn_transport_set_server() to mark it as a server. However if you use
 * pn_sasl() you *must* call call pn_transport_set_server() yourself *before*
 * calling pn_sasl() to set up a server SASL configuration.
 *
 * @param[in] listener the listener
 * @param[in] connection If NULL a new connection is created.
 * Memory management is the same as for pn_proactor_connect2()
 * @param[in] transport If NULL a new transport is created.
 * Memory management is the same as for pn_proactor_connect2()
 */
PNP_EXTERN void pn_listener_accept2(pn_listener_t *listener, pn_connection_t *connection, pn_transport_t *transport);

/**
 * **Deprecated** - Use ::pn_listener_accept2().
 */
PNP_EXTERN void pn_listener_accept(pn_listener_t* listener, pn_connection_t *connection);

/**
 * Get the error condition for a listener.
 */
PNP_EXTERN pn_condition_t *pn_listener_condition(pn_listener_t *l);

/**
 * Get the application context associated with this listener object.
 *
 * The application context for a connection may be set using
 * ::pn_listener_set_context.
 *
 * @param[in] listener the listener whose context is to be returned.
 * @return the application context for the listener object
 */
PNP_EXTERN void *pn_listener_get_context(pn_listener_t *listener);

/**
 * Set a new application context for a listener object.
 *
 * The application context for a listener object may be retrieved
 * using ::pn_listener_get_context.
 *
 * @param[in] listener the listener object
 * @param[in] context the application context
 */
PNP_EXTERN void pn_listener_set_context(pn_listener_t *listener, void *context);

/**
 * Get the attachments that are associated with a listener object.
 */
PNP_EXTERN pn_record_t *pn_listener_attachments(pn_listener_t *listener);

/**
 * Close the listener.
 * The PN_LISTENER_CLOSE event is generated when the listener has stopped listening.
 *
 * @note Thread safe. Must not be called after the PN_LISTENER_CLOSE event has
 * been handled as the listener may be freed .
 */
PNP_EXTERN void pn_listener_close(pn_listener_t *l);

/**
 * The proactor associated with a listener.
 */
PNP_EXTERN pn_proactor_t *pn_listener_proactor(pn_listener_t *c);

/**
 * Return the listener associated with an event.
 *
 * @return NULL if the event is not associated with a listener.
 */
PNP_EXTERN pn_listener_t *pn_event_listener(pn_event_t *event);

/**
 * Accept an incoming connection request as a raw connection.
 *
 * Call after a @ref PN_LISTENER_ACCEPT event.
 *
 * Errors are returned as @ref PN_RAW_CONNECTION_DISCONNECTED by pn_proactor_wait().
 *
 * @param[in] listener the listener
 * @param[in] raw_connection the application must create a raw connection with pn_raw_connection()
 * this parameter cannot be null.If NULL a new connection is created.
 *
 * The proactor that owns the @p listener *takes ownership* of @p raw_connection and will
 * automatically call pn_raw_connection_free() after the final @ref
 * PN_RAW_CONNECTION_DISCONNECTED event is handled, or when pn_proactor_free() is
 * called.
 *
 */
PNP_EXTERN void pn_listener_raw_accept(pn_listener_t *listener, pn_raw_connection_t *raw_connection);

/**
 *@}
 */

#ifdef __cplusplus
}
#endif

#endif /* listener.h */
