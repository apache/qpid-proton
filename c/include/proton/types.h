#ifndef PROTON_TYPES_H
#define PROTON_TYPES_H 1

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
#include <stddef.h>
#include <proton/type_compat.h>

/**
 * @file
 *
 * @copybrief types
 *
 * @defgroup core Core
 * @brief Core protocol entities and event handling.
 *
 * @defgroup connection Connection
 * @brief A connection to a remote AMQP peer.
 * @ingroup core
 *
 * @defgroup session Session
 * @brief A container of links.
 * @ingroup core
 *
 * @defgroup link Link
 * @brief A channel for transferring messages.
 * @ingroup core
 *
 * @defgroup terminus Terminus
 * @brief A source or target for messages.
 * @ingroup core
 *
 * @defgroup message Message
 * @brief A mutable holder of application content.
 * @ingroup core
 *
 * @defgroup delivery Delivery
 * @brief A message transfer.
 * @ingroup core
 *
 * @defgroup condition Condition
 * @brief An endpoint error state.
 * @ingroup core
 *
 * @defgroup event Event
 * @brief Protocol and transport events.
 * @ingroup core
 *
 * @defgroup transport Transport
 * @brief A network channel supporting an AMQP connection.
 * @ingroup core
 *
 * @defgroup sasl SASL
 * @brief SASL secure transport layer.
 * @ingroup core
 *
 * @defgroup ssl SSL
 * @brief SSL secure transport layer.
 * @ingroup core
 *
 * @defgroup error Error
 * @brief A Proton API error.
 * @ingroup core
 *
 * @defgroup types Types
 * @brief AMQP and API data types.
 *
 * @defgroup amqp_types AMQP data types
 * @brief AMQP data types.
 * @ingroup types
 *
 * @defgroup api_types API data types
 * @brief Additional data types used in the API.
 * @ingroup types
 *
 * @defgroup codec Codec
 * @brief AMQP data encoding and decoding.
 *
 * @defgroup data Data
 * @brief A data structure for AMQP data.
 * @ingroup codec
 *
 * @defgroup io IO
 * @brief **Unsettled API** - Interfaces for IO integration.
 *
 * @defgroup proactor Proactor
 * @brief **Unsettled API** - An API for multithreaded IO.
 * @ingroup io
 *
 * @defgroup proactor_events Proactor events
 * @brief **Unsettled API** - Events used by the proactor.
 * @ingroup io
 *
 * @defgroup listener Listener
 * @brief **Unsettled API** - A listener for incoming connections.
 * @ingroup io
 *
 * @defgroup raw_connection Raw connection
 * @brief **Unsettled API** - An API allowing raw sockets to be used with proactor
 * @ingroup io
 *
 * @defgroup connection_driver Connection driver
 * @brief **Unsettled API** - An API for low-level IO integration.
 * @ingroup io
 *
 * @defgroup messenger Messenger
 * @brief **Deprecated** - Use the @ref proactor API or Qpid Proton C++.
 *
 * @defgroup url URL
 * @brief **Deprecated** - Use a third-party URL library.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A sequence number.
 *
 * @ingroup api_types
 */
typedef uint32_t  pn_sequence_t;

/**
 * A span of time in milliseconds.
 *
 * @ingroup api_types
 */
typedef uint32_t pn_millis_t;

/**
 * The maximum value for @ref pn_millis_t.
 *
 * @ingroup api_types
 */
#define PN_MILLIS_MAX (~0U)

/**
 * A span of time in seconds.
 *
 * @ingroup api_types
 */
typedef uint32_t pn_seconds_t;

/**
 * A 64-bit timestamp in milliseconds since the Unix epoch.
 *
 * @ingroup amqp_types
 */
typedef int64_t pn_timestamp_t;

/**
 * A 32-bit Unicode code point.
 *
 * @ingroup amqp_types
 */
typedef uint32_t pn_char_t;

/**
 * A 32-bit decimal floating-point number.
 *
 * @ingroup amqp_types
 */
typedef uint32_t pn_decimal32_t;

/**
 * A 64-bit decimal floating-point number.
 *
 * @ingroup amqp_types
 */
typedef uint64_t pn_decimal64_t;

/**
 * A 128-bit decimal floating-point number.
 *
 * @ingroup amqp_types
 */
typedef struct {
  char bytes[16];
} pn_decimal128_t;

/**
 * A 16-byte universally unique identifier.
 *
 * @ingroup amqp_types
 */
typedef struct {
  char bytes[16];
} pn_uuid_t;

/**
 * A const byte buffer.
 *
 * @ingroup api_types
 */
typedef struct pn_bytes_t {
  size_t size;
  const char *start;
} pn_bytes_t;

/**
 * Create a @ref pn_bytes_t
 *
 * @ingroup api_types
 */
PN_EXTERN pn_bytes_t pn_bytes(size_t size, const char *start);

PN_EXTERN extern const pn_bytes_t pn_bytes_null;

/**
 * A non-const byte buffer.
 *
 * @ingroup api_types
 */
typedef struct pn_rwbytes_t {
  size_t size;
  char *start;
} pn_rwbytes_t;

/**
 * Create a @ref pn_rwbytes_t
 *
 * @ingroup api_types
 */
PN_EXTERN pn_rwbytes_t pn_rwbytes(size_t size, char *start);

PN_EXTERN extern const pn_rwbytes_t pn_rwbytes_null;

/**
 * Holds the state flags for an AMQP endpoint.
 *
 * A pn_state_t is an integral value with flags that encode both the
 * local and remote state of an AMQP Endpoint (@link pn_connection_t
 * Connection @endlink, @link pn_session_t Session @endlink, or @link
 * pn_link_t Link @endlink). The local portion of the state may be
 * accessed using ::PN_LOCAL_MASK, and the remote portion may be
 * accessed using ::PN_REMOTE_MASK. Individual bits may be accessed
 * using ::PN_LOCAL_UNINIT, ::PN_LOCAL_ACTIVE, ::PN_LOCAL_CLOSED, and
 * ::PN_REMOTE_UNINIT, ::PN_REMOTE_ACTIVE, ::PN_REMOTE_CLOSED.
 *
 * Every AMQP endpoint (@link pn_connection_t Connection @endlink,
 * @link pn_session_t Session @endlink, or @link pn_link_t Link
 * @endlink) starts out in an uninitialized state and then proceeds
 * linearly to an active and then closed state. This lifecycle occurs
 * at both endpoints involved, and so the state model for an endpoint
 * includes not only the known local state, but also the last known
 * state of the remote endpoint.
 *
 * @ingroup connection
 */
typedef int pn_state_t;

/**
 * An AMQP Connection object.
 *
 * A pn_connection_t object encapsulates all of the endpoint state
 * associated with an AMQP Connection. A pn_connection_t object
 * contains zero or more ::pn_session_t objects, which in turn contain
 * zero or more ::pn_link_t objects. Each ::pn_link_t object contains
 * an ordered sequence of ::pn_delivery_t objects. A link is either a
 * sender or a receiver but never both.
 *
 * @ingroup connection
 */
typedef struct pn_connection_t pn_connection_t;

/**
 * An AMQP Session object.
 *
 * A pn_session_t object encapsulates all of the endpoint state
 * associated with an AMQP Session. A pn_session_t object contains
 * zero or more ::pn_link_t objects.
 *
 * @ingroup session
 */
typedef struct pn_session_t pn_session_t;

/**
 * An AMQP Link object.
 *
 * A pn_link_t object encapsulates all of the endpoint state
 * associated with an AMQP Link. A pn_link_t object contains an
 * ordered sequence of ::pn_delivery_t objects representing in-flight
 * deliveries. A pn_link_t may be either sender or a receiver but
 * never both.
 *
 * A pn_link_t object maintains a pointer to the *current* delivery
 * within the ordered sequence of deliveries contained by the link
 * (See ::pn_link_current). The *current* delivery is the target of a
 * number of operations associated with the link, such as sending
 * (::pn_link_send) and receiving (::pn_link_recv) message data.
 *
 * @ingroup link
 */
typedef struct pn_link_t pn_link_t;

/**
 * An AMQP Delivery object.
 *
 * A pn_delivery_t object encapsulates all of the endpoint state
 * associated with an AMQP Delivery. Every delivery exists within the
 * context of a ::pn_link_t object.
 *
 * The AMQP model for settlement is based on the lifecycle of a
 * delivery at an endpoint. At each end of a link, a delivery is
 * created, it exists for some period of time, and finally it is
 * forgotten, aka settled. Note that because this lifecycle happens
 * independently at both the sender and the receiver, there are
 * actually four events of interest in the combined lifecycle of a
 * given delivery:
 *
 *   - created at sender
 *   - created at receiver
 *   - settled at sender
 *   - settled at receiver
 *
 * Because the sender and receiver are operating concurrently, these
 * events can occur in a variety of different orders, and the order of
 * these events impacts the types of failures that may occur when
 * transferring a delivery. Eliminating scenarios where the receiver
 * creates the delivery first, we have the following possible
 * sequences of interest:
 *
 * Sender presettles (aka at-most-once):
 * -------------------------------------
 *
 *   1. created at sender
 *   2. settled at sender
 *   3. created at receiver
 *   4. settled at receiver
 *
 * In this configuration the sender settles (i.e. forgets about) the
 * delivery before it even reaches the receiver, and if anything
 * should happen to the delivery in-flight, there is no way to
 * recover, hence the "at most once" semantics.
 *
 * Receiver settles first (aka at-least-once):
 * -------------------------------------------
 *
 *   1. created at sender
 *   2. created at receiver
 *   3. settled at receiver
 *   4. settled at sender
 *
 * In this configuration the receiver settles the delivery first, and
 * the sender settles once it sees the receiver has settled. Should
 * anything happen to the delivery in-flight, the sender can resend,
 * however the receiver may have already forgotten the delivery and so
 * it could interpret the resend as a new delivery, hence the "at
 * least once" semantics.
 *
 * Receiver settles second (aka exactly-once):
 * -------------------------------------------
 *
 *   1. created at sender
 *   2. created at receiver
 *   3. settled at sender
 *   4. settled at receiver
 *
 * In this configuration the receiver settles only once it has seen
 * that the sender has settled. This provides the sender the option to
 * retransmit, and the receiver has the option to recognize (and
 * discard) duplicates, allowing for exactly once semantics.
 *
 * Note that in the last scenario the sender needs some way to know
 * when it is safe to settle. This is where delivery state comes in.
 * In addition to these lifecycle related events surrounding
 * deliveries there is also the notion of a delivery state that can
 * change over the lifetime of a delivery, e.g. it might start out as
 * nothing, transition to ::PN_RECEIVED and then transition to
 * ::PN_ACCEPTED. In the first two scenarios the delivery state isn't
 * required, however in final scenario the sender would typically
 * trigger settlement based on seeing the delivery state transition to
 * a terminal state like ::PN_ACCEPTED or ::PN_REJECTED.
 *
 * In practice settlement is controlled by application policy, so
 * there may well be more options here, e.g. a sender might not settle
 * strictly based on what has happened at the receiver, it might also
 * choose to impose some time limit and settle after that period has
 * expired, or it could simply have a sliding window of the last N
 * deliveries and settle the oldest whenever a new one comes along.
 *
 * @ingroup delivery
 */
typedef struct pn_delivery_t pn_delivery_t;

/**
 * An event collector.
 *
 * A pn_collector_t may be used to register interest in being notified
 * of high level events that can occur to the various objects
 * representing AMQP endpoint state. See ::pn_event_t for more
 * details.
 *
 * @ingroup event
 */
typedef struct pn_collector_t pn_collector_t;

/**
 * A listener for incoming connections.
 *
 * @ingroup listener
 */
typedef struct pn_listener_t pn_listener_t;

/**
 * A network channel supporting an AMQP connection.
 *
 * A pn_transport_t encapsulates the transport related state of all
 * AMQP endpoint objects associated with a physical network connection
 * at a given point in time.
 *
 * @ingroup transport
 */
typedef struct pn_transport_t pn_transport_t;

/**
 * A harness for multithreaded IO.
 *
 * @ingroup proactor
 */
typedef struct pn_proactor_t pn_proactor_t;

/**
 * A raw network connection used with the proactor.
 *
 * @ingroup raw_connection
 */
typedef struct pn_raw_connection_t pn_raw_connection_t;

/**
 * A batch of events that must be handled in sequence.
 *
 * A pn_event_batch_t encapsulates potentially multiple events that relate
 * to an individual proactor related source that must be handled in sequence.
 *
 * Call pn_event_batch_next() in a loop until it returns NULL to extract
 * the batch's events.
 *
 * @ingroup proactor
 */
typedef struct pn_event_batch_t pn_event_batch_t;

/**
 * @cond INTERNAL
 *
 * An event handler
 *
 * A pn_handler_t is target of ::pn_event_t dispatched by the pn_reactor_t
 */
typedef struct pn_handler_t pn_handler_t;
/**
 * @endcond
 */

#ifdef __cplusplus
}
#endif

#endif /* types.h */
