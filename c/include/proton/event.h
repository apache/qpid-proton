#ifndef PROTON_EVENT_H
#define PROTON_EVENT_H 1

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

#include <proton/import_export.h>
#include <proton/type_compat.h>
#include <proton/object.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 *
 * @copybrief event
 *
 * @addtogroup event
 * @{
 */

/**
 * Notification of a state change in the protocol engine.
 *
 * The AMQP endpoint state modeled by the protocol engine is captured
 * by the following object types: @link pn_delivery_t Deliveries
 * @endlink, @link pn_link_t Links @endlink, @link pn_session_t
 * Sessions @endlink, @link pn_connection_t Connections @endlink, and
 * @link pn_transport_t Transports @endlink. These objects are related
 * as follows:
 *
 * - @link pn_delivery_t Deliveries @endlink always have a single
 *   parent Link
 * - @link pn_link_t Links @endlink always have a single parent
 *   Session
 * - @link pn_session_t Sessions @endlink always have a single parent
 *   Connection
 * - @link pn_connection_t Connections @endlink optionally have at
 *   most one associated Transport
 * - @link pn_transport_t Transports @endlink optionally have at most
 *   one associated Connection
 *
 * Every event has a type (see ::pn_event_type_t) that identifies what
 * sort of state change has occurred along with a pointer to the
 * object whose state has changed (as well as its associated objects).
 *
 * Events are accessed by creating a @link pn_collector_t Collector
 * @endlink with ::pn_collector() and registering it with the @link
 * pn_connection_t Connection @endlink of interest through use of
 * ::pn_connection_collect(). Once a collector has been registered,
 * ::pn_collector_peek() and ::pn_collector_pop() are used to access
 * and process events.
 */
typedef struct pn_event_t pn_event_t;

/**
 * An event type.
 */
typedef enum {
  /**
   * Defined as a programming convenience. No event of this type will
   * ever be generated.
   */
  PN_EVENT_NONE = 0,

  /**
   * A reactor has been started. Events of this type point to the reactor.
   */
  PN_REACTOR_INIT,

  /**
   * A reactor has no more events to process. Events of this type
   * point to the reactor.
   */
  PN_REACTOR_QUIESCED,

  /**
   * A reactor has been stopped. Events of this type point to the reactor.
   */
  PN_REACTOR_FINAL,

  /**
   * A timer event has occurred.
   */
  PN_TIMER_TASK,

  /**
   * The connection has been created. This is the first event that
   * will ever be issued for a connection. Events of this type point
   * to the relevant connection.
   */
  PN_CONNECTION_INIT,

  /**
   * The connection has been bound to a transport. This event is
   * issued when the ::pn_transport_bind() operation is invoked.
   */
  PN_CONNECTION_BOUND,

  /**
   * The connection has been unbound from its transport. This event is
   * issued when the ::pn_transport_unbind() operation is invoked.
   */
  PN_CONNECTION_UNBOUND,

  /**
   * The local connection endpoint has been closed. Events of this
   * type point to the relevant connection.
   */
  PN_CONNECTION_LOCAL_OPEN,

  /**
   * The remote endpoint has opened the connection. Events of this
   * type point to the relevant connection.
   */
  PN_CONNECTION_REMOTE_OPEN,

  /**
   * The local connection endpoint has been closed. Events of this
   * type point to the relevant connection.
   */
  PN_CONNECTION_LOCAL_CLOSE,

  /**
   *  The remote endpoint has closed the connection. Events of this
   *  type point to the relevant connection.
   */
  PN_CONNECTION_REMOTE_CLOSE,

  /**
   * The connection has been freed and any outstanding processing has
   * been completed. This is the final event that will ever be issued
   * for a connection.
   */
  PN_CONNECTION_FINAL,

  /**
   * The session has been created. This is the first event that will
   * ever be issued for a session.
   */
  PN_SESSION_INIT,

  /**
   * The local session endpoint has been opened. Events of this type
   * point to the relevant session.
   */
  PN_SESSION_LOCAL_OPEN,

  /**
   * The remote endpoint has opened the session. Events of this type
   * point to the relevant session.
   */
  PN_SESSION_REMOTE_OPEN,

  /**
   * The local session endpoint has been closed. Events of this type
   * point ot the relevant session.
   */
  PN_SESSION_LOCAL_CLOSE,

  /**
   * The remote endpoint has closed the session. Events of this type
   * point to the relevant session.
   */
  PN_SESSION_REMOTE_CLOSE,

  /**
   * The session has been freed and any outstanding processing has
   * been completed. This is the final event that will ever be issued
   * for a session.
   */
  PN_SESSION_FINAL,

  /**
   * The link has been created. This is the first event that will ever
   * be issued for a link.
   */
  PN_LINK_INIT,

  /**
   * The local link endpoint has been opened. Events of this type
   * point ot the relevant link.
   */
  PN_LINK_LOCAL_OPEN,

  /**
   * The remote endpoint has opened the link. Events of this type
   * point to the relevant link.
   */
  PN_LINK_REMOTE_OPEN,

  /**
   * The local link endpoint has been closed. Events of this type
   * point to the relevant link.
   */
  PN_LINK_LOCAL_CLOSE,

  /**
   * The remote endpoint has closed the link. Events of this type
   * point to the relevant link.
   */
  PN_LINK_REMOTE_CLOSE,

  /**
   * The local link endpoint has been detached. Events of this type
   * point to the relevant link.
   */
  PN_LINK_LOCAL_DETACH,

  /**
   * The remote endpoint has detached the link. Events of this type
   * point to the relevant link.
   */
  PN_LINK_REMOTE_DETACH,

  /**
   * The flow control state for a link has changed. Events of this
   * type point to the relevant link.
   */
  PN_LINK_FLOW,

  /**
   * The link has been freed and any outstanding processing has been
   * completed. This is the final event that will ever be issued for a
   * link. Events of this type point to the relevant link.
   */
  PN_LINK_FINAL,

  /**
   * A delivery has been created or updated. Events of this type point
   * to the relevant delivery.
   */
  PN_DELIVERY,

  /**
   * The transport has new data to read and/or write. Events of this
   * type point to the relevant transport.
   */
  PN_TRANSPORT,

  /**
   * The transport has authenticated. If this is received by a server
   * the associated transport has authenticated an incoming connection
   * and pn_transport_get_user() can be used to obtain the authenticated
   * user.
   */
  PN_TRANSPORT_AUTHENTICATED,

  /**
   * Indicates that a transport error has occurred. Use
   * ::pn_transport_condition() to access the details of the error
   * from the associated transport.
   */
  PN_TRANSPORT_ERROR,

  /**
   * Indicates that the "head" or writing end of the transport has been closed. This
   * means the transport will never produce more bytes for output to
   * the network. Events of this type point to the relevant transport.
   */
  PN_TRANSPORT_HEAD_CLOSED,

  /**
   * Indicates that the tail of the transport has been closed. This
   * means the transport will never be able to process more bytes from
   * the network. Events of this type point to the relevant transport.
   */
  PN_TRANSPORT_TAIL_CLOSED,

  /**
   * Indicates that the both the head and tail of the transport are
   * closed. Events of this type point to the relevant transport.
   */
  PN_TRANSPORT_CLOSED,

  PN_SELECTABLE_INIT,
  PN_SELECTABLE_UPDATED,
  PN_SELECTABLE_READABLE,
  PN_SELECTABLE_WRITABLE,
  PN_SELECTABLE_ERROR,
  PN_SELECTABLE_EXPIRED,
  PN_SELECTABLE_FINAL,

  /**
   * pn_connection_wake() was called.
   * Events of this type point to the @ref pn_connection_t.
   */
  PN_CONNECTION_WAKE,

  /**
   * Indicates the listener has an incoming connection, call pn_listener_accept2()
   * to accept it.
   * Events of this type point to the @ref pn_listener_t.
   */
  PN_LISTENER_ACCEPT,

  /**
   * Indicates the listener has closed. pn_listener_condition() provides error information.
   * Events of this type point to the @ref pn_listener_t.
   */
  PN_LISTENER_CLOSE,

  /**
   * Indicates pn_proactor_interrupt() was called to interrupt a proactor thread.
   * Events of this type point to the @ref pn_proactor_t.
   */
  PN_PROACTOR_INTERRUPT,

  /**
   * Timeout set by pn_proactor_set_timeout() time limit expired.
   * Events of this type point to the @ref pn_proactor_t.
   */
  PN_PROACTOR_TIMEOUT,

  /**
   * The proactor has become inactive: all listeners and connections were closed
   * and the timeout (if set) expired or was cancelled. There will be no
   * further events unless new listeners or connections are opened, or a new
   * timeout is set (possibly in other threads in a multi-threaded program.)
   *
   * Events of this type point to the @ref pn_proactor_t.
   */
  PN_PROACTOR_INACTIVE,

  /**
   * The listener is listening.
   * Events of this type point to the @ref pn_listener_t.
   */
  PN_LISTENER_OPEN,

  /**
   * The raw connection connected.
   * Now would be a good time to give the raw connection some buffers
   * to read bytes from the underlying socket. If you don't do it
   * now you will get @ref PN_RAW_CONNECTION_NEED_READ_BUFFERS (and
   * @ref PN_RAW_CONNECTION_NEED_WRITE_BUFFERS) events when the socket is readable
   * (or writable) but there are not buffers available.
   *
   * Events of this type point to a @ref pn_raw_connection_t
   */
  PN_RAW_CONNECTION_CONNECTED,

  /**
   * The remote end of the raw connection closed the connection so
   * that we can no longer read.
   *
   * When both this and @ref PN_RAW_CONNECTION_CLOSED_WRITE event have
   * occurred then the @ref PN_RAW_CONNECTION_DISCONNECTED event will be
   * generated.
   *
   * Events of this type point to a @ref pn_raw_connection_t
   */
  PN_RAW_CONNECTION_CLOSED_READ,

  /**
   * The remote end of the raw connection closed the connection so
   * that we can no longer write.
   *
   * When both this and @ref PN_RAW_CONNECTION_CLOSED_READ event have
   * occurred then the @ref PN_RAW_CONNECTION_DISCONNECTED event will be
   * generated.
   *
   * Events of this type point to a @ref pn_raw_connection_t
   */
  PN_RAW_CONNECTION_CLOSED_WRITE,

  /**
   * The raw connection is disconnected.
   * No more bytes will be read or written on the connection. Every buffer
   * in use will already either have been returned using a
   * @ref PN_RAW_CONNECTION_READ or @ref PN_RAW_CONNECTION_WRITTEN event.
   * This event will always be the last for this raw connection, and so
   * the application can clean up the raw connection at this point.
   *
   * Events of this type point to a @ref pn_raw_connection_t
   */
  PN_RAW_CONNECTION_DISCONNECTED,

  /**
   * The raw connection might need more read buffers.
   * The connection is readable, but the connection has no buffer to read the
   * bytes into. If you supply some buffers now maybe you'll get a
   * @ref PN_RAW_CONNECTION_READ event soon, but no guarantees.
   *
   * This event is edge triggered and you will only get it once until you give
   * the raw connection some more read buffers.
   *
   * Events of this type point to a @ref pn_raw_connection_t
   */
  PN_RAW_CONNECTION_NEED_READ_BUFFERS,

  /**
   * The raw connection might need more write buffers.
   * The connection is writable but has no buffers to write. If you give the
   * raw connection something to write using @ref pn_raw_connection_write_buffers
   * the raw connection can write them. It is not necessary to wait for this event
   * before sending buffers to write, but it can be used to aid in flow control (maybe).
   *
   * This event is edge triggered and you will only get it once until you give
   * the raw connection something more to write.
   *
   * Events of this type point to a @ref pn_raw_connection_t
   */
  PN_RAW_CONNECTION_NEED_WRITE_BUFFERS,

  /**
   * The raw connection read bytes: The bytes that were read are
   * in one of the read buffers given to the raw connection.
   *
   * This event will be sent if there are bytes that have been read
   * in a buffer owned by the raw connection and there is no
   * @ref PN_RAW_CONNECTION_READ event still queued.
   *
   * When a connection closes all read buffers are returned to the
   * application using @ref PN_RAW_CONNECTION_READ events with empty buffers.
   *
   * Events of this type point to a @ref pn_raw_connection_t
   */
  PN_RAW_CONNECTION_READ,

  /**
   * The raw connection has finished a write and the buffers that were
   * used are no longer in use and can be recycled.
   *
   * This event will be sent if there are buffers that have been written still
   * owned by the raw connection and there is no @ref PN_RAW_CONNECTION_WRITTEN
   * event currently queued.
   *
   * When a connection closes all write buffers are returned using
   * @ref PN_RAW_CONNECTION_WRITTEN events.
   *
   * Events of this type point to a @ref pn_raw_connection_t
   */
  PN_RAW_CONNECTION_WRITTEN,

  /**
   * The raw connection was woken by @ref pn_raw_connection_wake
   *
   * Events of this type point to a @ref pn_raw_connection_t
   */
  PN_RAW_CONNECTION_WAKE,

  /**
   * The raw connection is returning all the remaining buffers to the application.
   *
   * The raw connection is about to disconnect and shutdown. To avoid leaking the buffers
   * the application must take the buffers back used @ref pn_raw_connection_take_read_buffers
   * and @ref pn_raw_connection_take_write_buffers.
   *
   * Events of this type point to a @ref pn_raw_connection_t
   */
  PN_RAW_CONNECTION_DRAIN_BUFFERS

} pn_event_type_t;


/**
 * Get a human readable name for an event type.
 *
 * @param[in] type an event type
 * @return a human readable name
 */
PN_EXTERN const char *pn_event_type_name(pn_event_type_t type);

/**
 * Construct a collector.
 *
 * A collector is used to register interest in events produced by one
 * or more ::pn_connection_t objects. Collectors are not currently
 * thread safe, so synchronization must be used if they are to be
 * shared between multiple connection objects.
 */
PN_EXTERN pn_collector_t *pn_collector(void);

/**
 * Free a collector.
 *
 * @param[in] collector a collector to free, or NULL
 */
PN_EXTERN void pn_collector_free(pn_collector_t *collector);

/**
 * Release a collector. Once in a released state a collector will
 * drain any internally queued events (thereby releasing any pointers
 * they may hold), shrink it's memory footprint to a minimum, and
 * discard any newly created events.
 *
 * @param[in] collector a collector object
 */
PN_EXTERN void pn_collector_release(pn_collector_t *collector);

/**
 * Drain a collector: remove and discard all events.
 *
 * @param[in] collector a collector object
 */
PN_EXTERN void pn_collector_drain(pn_collector_t *collector);

/**
 * Place a new event on a collector.
 *
 * This operation will create a new event of the given type and
 * context and return a pointer to the newly created event. In some
 * cases an event of the given type and context can be elided. When
 * this happens, this operation will return a NULL pointer.
 *
 * @param[in] collector a collector object
 * @param[in] clazz class of the context
 * @param[in] context the event context
 * @param[in] type the event type
 *
 * @return a pointer to the newly created event or NULL if the event
 *         was elided
 */

PN_EXTERN pn_event_t *pn_collector_put(pn_collector_t *collector,
                                       const pn_class_t *clazz, void *context,
                                       pn_event_type_t type);

/**
 * Access the head event contained by a collector.
 *
 * This operation will continue to return the same event until it is
 * cleared by using ::pn_collector_pop. The pointer return by this
 * operation will be valid until ::pn_collector_pop is invoked or
 * ::pn_collector_free is called, whichever happens sooner.
 *
 * @param[in] collector a collector object
 * @return a pointer to the head event contained in the collector
 */
PN_EXTERN pn_event_t *pn_collector_peek(pn_collector_t *collector);

/**
 * Remove the head event on a collector.
 *
 * @param[in] collector a collector object
 * @return true if the event was popped, false if the collector is empty
 */
PN_EXTERN bool pn_collector_pop(pn_collector_t *collector);

/**
 * Pop and return the head event, returns NULL if the collector is empty.
 * The returned pointer is valid till the next call of pn_collector_next().
 *
 * @param[in] collector a collector object
 * @return the next event.
 */
PN_EXTERN pn_event_t *pn_collector_next(pn_collector_t *collector);

/**
 * Return the same pointer as the most recent call to pn_collector_next().
 *
 * @param[in] collector a collector object
 * @return a pointer to the event returned by previous call to pn_collector_next()
 */
PN_EXTERN pn_event_t *pn_collector_prev(pn_collector_t *collector);

/**
 * Check if there are more events after the current head event. If this
 * returns true, then pn_collector_peek() will return an event even
 * after pn_collector_pop() is called.
 *
 * @param[in] collector a collector object
 * @return true if the collector has more than the current event
 */
PN_EXTERN  bool pn_collector_more(pn_collector_t *collector);

/**
 * Get the type of an event.
 *
 * @param[in] event an event object
 * @return the type of the event
 */
PN_EXTERN pn_event_type_t pn_event_type(pn_event_t *event);

/**
 * Get the class associated with the event context.
 *
 * @param[in] event an event object
 * @return the class associated with the event context
 */
PN_EXTERN const pn_class_t *pn_event_class(pn_event_t *event);

/**
 * Get the context associated with an event.
 */
PN_EXTERN void *pn_event_context(pn_event_t *event);

/**
 * Get the connection associated with an event.
 *
 * @param[in] event an event object
 * @return the connection associated with the event (or NULL)
 */
PN_EXTERN pn_connection_t *pn_event_connection(pn_event_t *event);

/**
 * Get the session associated with an event.
 *
 * @param[in] event an event object
 * @return the session associated with the event (or NULL)
 */
PN_EXTERN pn_session_t *pn_event_session(pn_event_t *event);

/**
 * Get the link associated with an event.
 *
 * @param[in] event an event object
 * @return the link associated with the event (or NULL)
 */
PN_EXTERN pn_link_t *pn_event_link(pn_event_t *event);

/**
 * Get the delivery associated with an event.
 *
 * @param[in] event an event object
 * @return the delivery associated with the event (or NULL)
 */
PN_EXTERN pn_delivery_t *pn_event_delivery(pn_event_t *event);

/**
 * Get the transport associated with an event.
 *
 * @param[in] event an event object
 * @return the transport associated with the event (or NULL)
 */
PN_EXTERN pn_transport_t *pn_event_transport(pn_event_t *event);

/**
 * Get any attachments associated with an event.
 *
 * @param[in] event an event object
 * @return the record holding the attachments
 */
PN_EXTERN pn_record_t *pn_event_attachments(pn_event_t *event);

/**
 * If the event context object has a condition and the condition is set
 * return it, otherwise return NULL.
 * If the event context object has remote and local conditions,
 * try the remote condition first, then the local.
 */
PN_EXTERN struct pn_condition_t *pn_event_condition(pn_event_t *event);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* event.h */
