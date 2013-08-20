#ifndef PROTON_MESSENGER_H
#define PROTON_MESSENGER_H 1

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
#include <proton/message.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @file
 * The messenger API provides a high level interface for sending and
 * receiving AMQP messages.
 */

typedef struct pn_messenger_t pn_messenger_t; /**< Messenger*/
typedef struct pn_subscription_t pn_subscription_t; /**< Subscription*/
typedef int64_t pn_tracker_t;

typedef enum {
  PN_STATUS_UNKNOWN = 0,
  PN_STATUS_PENDING = 1,
  PN_STATUS_ACCEPTED = 2,
  PN_STATUS_REJECTED = 3,
  PN_STATUS_MODIFIED = 4
} pn_status_t;

/** Construct a new Messenger with the given name. The name is global.
 * If a NULL name is supplied, a UUID based name will be chosen.
 *
 * @param[in] name the name of the messenger or NULL
 *
 * @return pointer to a new Messenger
 */
PN_EXTERN pn_messenger_t *pn_messenger(const char *name);

/** Retrieves the name of a Messenger.
 *
 * @param[in] messenger the messenger
 *
 * @return the name of the messenger
 */
PN_EXTERN const char *pn_messenger_name(pn_messenger_t *messenger);

/** Provides a certificate that will be used to identify the local
 * Messenger to the peer.
 *
 * @param[in] messenger the messenger
 * @param[in] certificate a path to a certificate file
 *
 * @return an error code of zero if there is no error
 */
PN_EXTERN int pn_messenger_set_certificate(pn_messenger_t *messenger, const char *certificate);

/** Gets the certificate file for a Messenger.
 *
 * @param[in] messenger the messenger
 * @return the certificate file path
 */
PN_EXTERN const char *pn_messenger_get_certificate(pn_messenger_t *messenger);

/** Provides the private key that was used to sign the certificate.
 * See ::pn_messenger_set_certificate
 *
 * @param[in] messenger the Messenger
 * @param[in] private_key a path to a private key file
 *
 * @return an error code of zero if there is no error
 */
PN_EXTERN int pn_messenger_set_private_key(pn_messenger_t *messenger, const char *private_key);

/** Gets the private key file for a Messenger.
 *
 * @param[in] messenger the messenger
 * @return the private key file path
 */
PN_EXTERN const char *pn_messenger_get_private_key(pn_messenger_t *messenger);

/** Sets the private key password for a Messenger.
 *
 * @param[in] messenger the messenger
 * @param[in] password the password for the private key file
 *
 * @return an error code of zero if there is no error
 */
PN_EXTERN int pn_messenger_set_password(pn_messenger_t *messenger, const char *password);

/** Gets the private key file password for a Messenger.
 *
 * @param[in] messenger the messenger
 * @return password for the private key file
 */
PN_EXTERN const char *pn_messenger_get_password(pn_messenger_t *messenger);

/** Sets the trusted certificates database for a Messenger.  Messenger
 * will use this database to validate the certificate provided by the
 * peer.
 *
 * @param[in] messenger the messenger
 * @param[in] cert_db a path to the certificates database
 *
 * @return an error code of zero if there is no error
 */
PN_EXTERN int pn_messenger_set_trusted_certificates(pn_messenger_t *messenger, const char *cert_db);

/** Gets the trusted certificates database for a Messenger.
 *
 * @param[in] messenger the messenger
 * @return path to the trusted certificates database
 */
PN_EXTERN const char *pn_messenger_get_trusted_certificates(pn_messenger_t *messenger);

/** Sets the timeout for a Messenger. A negative timeout means
 * infinite.
 *
 * @param[in] messenger the messenger
 * @param[in] timeout the new timeout for the messenger, in milliseconds
 *
 * @return an error code or zero if there is no error
 */
PN_EXTERN int pn_messenger_set_timeout(pn_messenger_t *messenger, int timeout);

/** Retrieves the timeout for a Messenger.
 *
 * @param[in] messenger the messenger
 *
 * @return the timeout for the messenger, in milliseconds
 */
PN_EXTERN int pn_messenger_get_timeout(pn_messenger_t *messenger);

PN_EXTERN bool pn_messenger_is_blocking(pn_messenger_t *messenger);
PN_EXTERN int pn_messenger_set_blocking(pn_messenger_t *messenger, bool blocking);

/** Frees a Messenger.
 *
 * @param[in] messenger the messenger to free, no longer valid on
 *                      return
 */
PN_EXTERN void pn_messenger_free(pn_messenger_t *messenger);

/** Returns the error code for the Messenger.
 *
 * @param[in] messenger the messenger to check for errors
 *
 * @return an error code or zero if there is no error
 * @see error.h
 */
PN_EXTERN int pn_messenger_errno(pn_messenger_t *messenger);

/** Returns the error info for a Messenger.
 *
 * @param[in] messenger the messenger to check for errors
 *
 * @return a descriptive error message or NULL if no error has
 *         occurred
 */
PN_EXTERN const char *pn_messenger_error(pn_messenger_t *messenger);

/** Gets the outgoing window for a Messenger. @see
 * ::pn_messenger_set_outgoing_window
 *
 * @param[in] messenger the messenger
 *
 * @return the outgoing window
 */
PN_EXTERN int pn_messenger_get_outgoing_window(pn_messenger_t *messenger);

/** Sets the outgoing window for a Messenger. If the outgoing window
 *  is set to a positive value, then after each call to
 *  pn_messenger_send, the Messenger will track the status of that
 *  many deliveries. @see ::pn_messenger_status
 *
 * @param[in] messenger the Messenger
 * @param[in] window the number of deliveries to track
 *
 * @return an error or zero on success
 * @see error.h
 */
PN_EXTERN int pn_messenger_set_outgoing_window(pn_messenger_t *messenger, int window);

/** Gets the incoming window for a Messenger. @see
 * ::pn_messenger_set_incoming_window
 *
 * @param[in] messenger the Messenger
 *
 * @return the incoming window
 */
PN_EXTERN int pn_messenger_get_incoming_window(pn_messenger_t *messenger);

/** Sets the incoming window for a Messenger. If the incoming window
 *  is set to a positive value, then after each call to
 *  pn_messenger_accept or pn_messenger_reject, the Messenger will
 *  track the status of that many deliveries. @see
 *  ::pn_messenger_status
 *
 * @param[in] messenger the Messenger
 * @param[in] window the number of deliveries to track
 *
 * @return an error or zero on success
 * @see error.h
 */
PN_EXTERN int pn_messenger_set_incoming_window(pn_messenger_t *messenger, int window);

/** Starts a messenger. A messenger cannot send or recv messages until
 * it is started.
 *
 * @param[in] messenger the messenger to start
 *
 * @return an error code or zero on success
 * @see error.h
 */
PN_EXTERN int pn_messenger_start(pn_messenger_t *messenger);

/** Stops a messenger. A messenger cannot send or recv messages when
 *  it is stopped.
 *
 * @param[in] messenger the messenger to stop
 *
 * @return an error code or zero on success
 * @see error.h
 */
PN_EXTERN int pn_messenger_stop(pn_messenger_t *messenger);

PN_EXTERN bool pn_messenger_stopped(pn_messenger_t *messenger);

/** Subscribes a messenger to messages from the specified source.
 *
 * @param[in] messenger the messenger to subscribe
 * @param[in] source
 *
 * @return a subscription
 */
PN_EXTERN pn_subscription_t *pn_messenger_subscribe(pn_messenger_t *messenger, const char *source);

PN_EXTERN void *pn_subscription_get_context(pn_subscription_t *sub);

PN_EXTERN void pn_subscription_set_context(pn_subscription_t *sub, void *context);

/** Puts a message on the outgoing message queue for a messenger.
 *
 * @param[in] messenger the messenger
 * @param[in] msg the message to put on the outgoing queue
 *
 * @return an error code or zero on success
 * @see error.h
 */
PN_EXTERN int pn_messenger_put(pn_messenger_t *messenger, pn_message_t *msg);

/** Gets the last known remote state of the delivery associated with
 * the given tracker.
 *
 * @param[in] messenger the messenger
 * @param[in] tracker the tracker identify the delivery
 *
 * @return a status code for the delivery
 */
PN_EXTERN pn_status_t pn_messenger_status(pn_messenger_t *messenger, pn_tracker_t tracker);

/** Frees a Messenger from tracking the status associated with a given
 * tracker. Use the PN_CUMULATIVE flag to indicate everything up to
 * (and including) the given tracker.
 *
 * @param[in] messenger the Messenger
 * @param[in] tracker identifies a delivery
 * @param[in] flags 0 or PN_CUMULATIVE
 *
 * @return an error code or zero on success
 * @see error.h
 */
PN_EXTERN int pn_messenger_settle(pn_messenger_t *messenger, pn_tracker_t tracker, int flags);

/** Gets the tracker for the message most recently provided to
 * pn_messenger_put.
 *
 * @param[in] messenger the messenger
 *
 * @return a pn_tracker_t or an undefined value if pn_messenger_get
 *         has never been called for the given messenger
 */
PN_EXTERN pn_tracker_t pn_messenger_outgoing_tracker(pn_messenger_t *messenger);

/** Sends or receives any outstanding messages queued for a messenger.
 * This will block for the indicated timeout.
 *
 * @param[in] messenger the Messenger
 * @param[in] timeout the maximum time to block
 */
PN_EXTERN int pn_messenger_work(pn_messenger_t *messenger, int timeout);

/** Interrupts a messenger that is blocking. This method may be safely
 * called from a different thread than the one that is blocking.
 *
 * @param[in] messenger the Messenger
 */
PN_EXTERN int pn_messenger_interrupt(pn_messenger_t *messenger);

/** Sends messages in the outgoing message queue for a messenger. This
 * call will block until the indicated number of messages have been
 * sent. If n is -1 this call will block until all outgoing messages
 * have been sent. If n is 0 then this call won't block.
 *
 * @param[in] messenger the messager
 * @param[in] n the number of messages to send
 *
 * @return an error code or zero on success
 * @see error.h
 */
PN_EXTERN int pn_messenger_send(pn_messenger_t *messenger, int n);

/** Instructs the messenger to receives up to limit messages into the
 * incoming message queue of a messenger. If limit is -1, Messenger
 * will receive as many messages as it can buffer internally. If the
 * messenger is in blocking mode, this call will block until at least
 * one message is available in the incoming queue.
 *
 * Each call to pn_messenger_recv replaces the previos receive
 * operation, so pn_messenger_recv(messenger, 0) will cancel any
 * outstanding receive.
 *
 * @param[in] messenger the messenger
 * @param[in] limit the maximum number of messages to receive or -1 to
 *                  to receive as many messages as it can buffer
 *                  internally.
 *
 * @return an error code or zero on success
 * @see error.h
 */
PN_EXTERN int pn_messenger_recv(pn_messenger_t *messenger, int limit);

/** Returns the number of messages currently being received by a
 * messenger.
 *
 * @param[in] messenger the messenger
 */
PN_EXTERN int pn_messenger_receiving(pn_messenger_t *messenger);

/** Gets a message from the head of the incoming message queue of a
 * messenger.
 *
 * @param[in] messenger the messenger
 * @param[out] msg upon return contains the message from the head of the queue
 *
 * @return an error code or zero on success
 * @see error.h
 */
PN_EXTERN int pn_messenger_get(pn_messenger_t *messenger, pn_message_t *msg);

/** Gets the tracker for the message most recently fetched by
 * pn_messenger_get.
 *
 * @param[in] messenger the messenger
 *
 * @return a pn_tracker_t or an undefined value if pn_messenger_get
 *         has never been called for the given messenger
 */
PN_EXTERN pn_tracker_t pn_messenger_incoming_tracker(pn_messenger_t *messenger);

PN_EXTERN pn_subscription_t *pn_messenger_incoming_subscription(pn_messenger_t *messenger);

#define PN_CUMULATIVE (0x1)

/** Accepts the incoming messages identified by the tracker. Use the
 * PN_CUMULATIVE flag to accept everything prior to the supplied
 * tracker.
 *
 * @param[in] messenger the messenger
 * @param[in] tracker an incoming tracker
 * @param[in] flags 0 or PN_CUMULATIVE
 *
 * @return an error code or zero on success
 * @see error.h
 */
PN_EXTERN int pn_messenger_accept(pn_messenger_t *messenger, pn_tracker_t tracker, int flags);

/** Rejects the incoming messages identified by the tracker. Use the
 * PN_CUMULATIVE flag to reject everything prior to the supplied
 * tracker.
 *
 * @param[in] messenger the Messenger
 * @param[in] tracker an incoming tracker
 * @param[in] flags 0 or PN_CUMULATIVE
 *
 * @return an error code or zero on success
 * @see error.h
 */
PN_EXTERN int pn_messenger_reject(pn_messenger_t *messenger, pn_tracker_t tracker, int flags);

/** Returns the number of messages in the outgoing message queue of a messenger.
 *
 * @param[in] messenger the Messenger
 *
 * @return the outgoing queue depth
 */
PN_EXTERN int pn_messenger_outgoing(pn_messenger_t *messenger);

/** Returns the number of messages in the incoming message queue of a messenger.
 *
 * @param[in] messenger the Messenger
 *
 * @return the incoming queue depth
 */
PN_EXTERN int pn_messenger_incoming(pn_messenger_t *messenger);

/** Adds a routing rule to a Messenger's internal routing table.
 *
 * The route procedure may be used to influence how a messenger will
 * internally treat a given address or class of addresses. Every call
 * to the route procedure will result in messenger appending a routing
 * rule to its internal routing table.
 *
 * Whenever a message is presented to a messenger for delivery, it
 * will match the address of this message against the set of routing
 * rules in order. The first rule to match will be triggered, and
 * instead of routing based on the address presented in the message,
 * the messenger will route based on the address supplied in the rule.
 *
 * The pattern matching syntax supports two types of matches, a '%'
 * will match any character except a '/', and a '*' will match any
 * character including a '/'.
 *
 * A routing address is specified as a normal AMQP address, however it
 * may additionally use substitution variables from the pattern match
 * that triggered the rule.
 *
 * Any message sent to "foo" will be routed to "amqp://foo.com":
 *
 *   pn_messenger_route("foo", "amqp://foo.com");
 *
 * Any message sent to "foobar" will be routed to
 * "amqp://foo.com/bar":
 *
 *   pn_messenger_route("foobar", "amqp://foo.com/bar");
 *
 * Any message sent to bar/<path> will be routed to the corresponding
 * path within the amqp://bar.com domain:
 *
 *   pn_messenger_route("bar/*", "amqp://bar.com/$1");
 *
 * Route all messages over TLS:
 *
 *   pn_messenger_route("amqp:*", "amqps:$1")
 *
 * Supply credentials for foo.com:
 *
 *   pn_messenger_route("amqp://foo.com/*", "amqp://user:password@foo.com/$1");
 *
 * Supply credentials for all domains:
 *
 *   pn_messenger_route("amqp://*", "amqp://user:password@$1");
 *
 * Route all addresses through a single proxy while preserving the
 * original destination:
 *
 *   pn_messenger_route("amqp://%/*", "amqp://user:password@proxy/$1/$2");
 *
 * Route any address through a single broker:
 *
 *   pn_messenger_route("*", "amqp://user:password@broker/$1");
 *
 * @param[in] messenger the Messenger
 * @param[in] pattern a glob pattern
 * @param[in] address an address indicating alternative routing
 *
 * @return an error code or zero on success
 * @see error.h
 *
 */
PN_EXTERN int pn_messenger_route(pn_messenger_t *messenger, const char *pattern,
                                 const char *address);

PN_EXTERN int pn_messenger_rewrite(pn_messenger_t *messenger, const char *pattern,
                                   const char *address);

#ifdef __cplusplus
}
#endif

#endif /* messenger.h */
