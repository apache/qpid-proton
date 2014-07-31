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
#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 *
 * Event API for the proton Engine.
 *
 * @defgroup event Event
 * @ingroup engine
 * @{
 */

/**
 * An event provides notification of a state change within the
 * protocol engine's object model.
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
 * Related events are grouped into categories
 */
typedef enum {
    PN_EVENT_CATEGORY_NONE   = 0,
    PN_EVENT_CATEGORY_CONNECTION = 0x00010000,
    PN_EVENT_CATEGORY_SESSION = 0x00020000,
    PN_EVENT_CATEGORY_LINK = 0x00030000,
    PN_EVENT_CATEGORY_DELIVERY = 0x00040000,
    PN_EVENT_CATEGORY_TRANSPORT = 0x00050000,
    PN_EVENT_CATEGORY_COUNT = 6
} pn_event_category_t;

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
   * The connection has been created. This is the first event that
   * will ever be issued for a connection. Events of this type point
   * to the relevant connection.
   */
  PN_CONNECTION_INIT = PN_EVENT_CATEGORY_CONNECTION + 1,

  /**
   * The local connection endpoint has been closed. Events of this
   * type point to the relevant connection.
   */
  PN_CONNECTION_OPEN = PN_EVENT_CATEGORY_CONNECTION + 2,

  /**
   * The remote endpoint has opened the connection. Events of this
   * type point to the relevant connection.
   */
  PN_CONNECTION_REMOTE_OPEN = PN_EVENT_CATEGORY_CONNECTION + 3,

  /**
   * The local connection endpoint has been closed. Events of this
   * type point to the relevant connection.
   */
  PN_CONNECTION_CLOSE = PN_EVENT_CATEGORY_CONNECTION + 4,

  /**
   *  The remote endpoint has closed the connection. Events of this
   *  type point to the relevant connection.
   */
  PN_CONNECTION_REMOTE_CLOSE = PN_EVENT_CATEGORY_CONNECTION + 5,

  /**
   * The connection has been freed and any outstanding processing has
   * been completed. This is the final event that will ever be issued
   * for a connection.
   */
  PN_CONNECTION_FINAL = PN_EVENT_CATEGORY_CONNECTION + 6,

  /**
   * The session has been created. This is the first event that will
   * ever be issued for a session.
   */
  PN_SESSION_INIT = PN_EVENT_CATEGORY_SESSION + 1,

  /**
   * The local session endpoint has been opened. Events of this type
   * point ot the relevant session.
   */
  PN_SESSION_OPEN = PN_EVENT_CATEGORY_SESSION + 2,

  /**
   * The remote endpoint has opened the session. Events of this type
   * point to the relevant session.
   */
  PN_SESSION_REMOTE_OPEN = PN_EVENT_CATEGORY_SESSION + 3,

  /**
   * The local session endpoint has been closed. Events of this type
   * point ot the relevant session.
   */
  PN_SESSION_CLOSE = PN_EVENT_CATEGORY_SESSION + 4,

  /**
   * The remote endpoint has closed the session. Events of this type
   * point to the relevant session.
   */
  PN_SESSION_REMOTE_CLOSE = PN_EVENT_CATEGORY_SESSION + 5,

  /**
   * The session has been freed and any outstanding processing has
   * been completed. This is the final event that will ever be issued
   * for a session.
   */
  PN_SESSION_FINAL = PN_EVENT_CATEGORY_SESSION + 6,

  /**
   * The link has been created. This is the first event that will ever
   * be issued for a link.
   */
  PN_LINK_INIT = PN_EVENT_CATEGORY_LINK + 1,

  /**
   * The local link endpoint has been opened. Events of this type
   * point ot the relevant link.
   */
  PN_LINK_OPEN = PN_EVENT_CATEGORY_LINK + 2,

  /**
   * The remote endpoint has opened the link. Events of this type
   * point to the relevant link.
   */
  PN_LINK_REMOTE_OPEN = PN_EVENT_CATEGORY_LINK + 3,

  /**
   * The local link endpoint has been closed. Events of this type
   * point ot the relevant link.
   */
  PN_LINK_CLOSE = PN_EVENT_CATEGORY_LINK + 4,

  /**
   * The remote endpoint has closed the link. Events of this type
   * point to the relevant link.
   */
  PN_LINK_REMOTE_CLOSE = PN_EVENT_CATEGORY_LINK + 5,

  /**
   * The flow control state for a link has changed. Events of this
   * type point to the relevant link.
   */
  PN_LINK_FLOW = PN_EVENT_CATEGORY_LINK + 6,

  /**
   * The link has been freed and any outstanding processing has been
   * completed. This is the final event that will ever be issued for a
   * link. Events of this type point to the relevant link.
   */
  PN_LINK_FINAL = PN_EVENT_CATEGORY_LINK + 7,

  /**
   * A delivery has been created or updated. Events of this type point
   * to the relevant delivery.
   */
  PN_DELIVERY = PN_EVENT_CATEGORY_DELIVERY + 1,

  /**
   * The transport has new data to read and/or write. Events of this
   * type point to the relevant transport.
   */
  PN_TRANSPORT = PN_EVENT_CATEGORY_TRANSPORT + 1

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
 * Clear the head event on a collector.
 *
 * @param[in] collector a collector object
 * @return true if the event was popped, false if the collector is empty
 */
PN_EXTERN bool pn_collector_pop(pn_collector_t *collector);

/**
 * Get the type of an event.
 *
 * @param[in] event an event object
 * @return the type of the event
 */
PN_EXTERN pn_event_type_t pn_event_type(pn_event_t *event);

/**
 * Get the category an event belongs to.
 *
 * @param[in] event an event object
 * @return the category the event belongs to
 */
PN_EXTERN pn_event_category_t pn_event_category(pn_event_t *event);

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

#ifdef __cplusplus
}
#endif

/** @}
 */

#endif /* event.h */
