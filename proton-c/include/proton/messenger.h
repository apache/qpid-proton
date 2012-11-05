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
  PN_ACCEPT_MODE_AUTO,
  PN_ACCEPT_MODE_MANUAL
} pn_accept_mode_t;

typedef enum {
  PN_STATUS_UNKNOWN = 0,
  PN_STATUS_PENDING = 1,
  PN_STATUS_ACCEPTED = 2,
  PN_STATUS_REJECTED = 3
} pn_status_t;

/** Construct a new Messenger with the given name. The name is global.
 * If a NULL name is supplied, a UUID based name will be chosen.
 *
 * @param[in] name the name of the messenger or NULL
 *
 * @return pointer to a new Messenger
 */
pn_messenger_t *pn_messenger(const char *name);

/** Retrieves the name of a Messenger.
 *
 * @param[in] messenger the messenger
 *
 * @return the name of the messenger
 */
const char *pn_messenger_name(pn_messenger_t *messenger);

/** Sets the certificate file for a Messenger.
 *
 * @param[in] messenger the messenger
 * @param[in] certificate a path to a certificate file
 *
 * @return an error code of zero if there is no error
 */
int pn_messenger_set_certificate(pn_messenger_t *messenger, const char *certificate);

/** Gets the certificate file fora Messenger.
 *
 * @param[in] messenger the messenger
 * @return the certificate file path
 */
const char *pn_messenger_get_certificate(pn_messenger_t *messenger);

/** Sets the private key file for a Messenger.
 *
 * @param[in] messenger the messenger
 * @param[in] private_key a path to a private key file
 *
 * @return an error code of zero if there is no error
 */
int pn_messenger_set_private_key(pn_messenger_t *messenger, const char *private_key);

/** Gets the private key file for a Messenger.
 *
 * @param[in] messenger the messenger
 * @return the private key file path
 */
const char *pn_messenger_get_private_key(pn_messenger_t *messenger);

/** Sets the private key password for a Messenger.
 *
 * @param[in] messenger the messenger
 * @param[in] password the password for the private key file
 *
 * @return an error code of zero if there is no error
 */
int pn_messenger_set_password(pn_messenger_t *messenger, const char *password);

/** Gets the private key file password for a Messenger.
 *
 * @param[in] messenger the messenger
 * @return password for the private key file
 */
const char *pn_messenger_get_password(pn_messenger_t *messenger);

/** Sets the trusted certificates database for a Messenger.
 *
 * @param[in] messenger the messenger
 * @param[in] cert_db a path to the certificates database
 *
 * @return an error code of zero if there is no error
 */
int pn_messenger_set_trusted_certificates(pn_messenger_t *messenger, const char *cert_db);

/** Gets the trusted certificates database for a Messenger.
 *
 * @param[in] messenger the messenger
 * @return path to the trusted certificates database
 */
const char *pn_messenger_get_trusted_certificates(pn_messenger_t *messenger);

/** Sets the timeout for a Messenger. A negative timeout means
 * infinite.
 *
 * @param[in] messenger the messenger
 * @param[timeout] the new timeout for the messenger, in milliseconds
 *
 * @return an error code or zero if there is no error
 */
int pn_messenger_set_timeout(pn_messenger_t *messenger, int timeout);

/** Retrieves the timeout for a Messenger.
 *
 * @param[in] messenger the messenger
 *
 * @return the timeout for the messenger, in milliseconds
 */
int pn_messenger_get_timeout(pn_messenger_t *messenger);

/** Frees a Messenger.
 *
 * @param[in] messenger the messenger to free, no longer valid on
 *                      return
 */
void pn_messenger_free(pn_messenger_t *messenger);

/** Returns the error code for the Messenger.
 *
 * @param[in] messenger the messenger to check for errors
 *
 * @return an error code or zero if there is no error
 * @see error.h
 */
int pn_messenger_errno(pn_messenger_t *messenger);

/** Returns the error info for the Messenger.
 *
 * @param[in] messenger the messenger to check for errors
 *
 * @return a descriptive error message or NULL if no error has
 *         occurred
 */
const char *pn_messenger_error(pn_messenger_t *messenger);

pn_accept_mode_t pn_messenger_get_accept_mode(pn_messenger_t *messenger);

int pn_messenger_set_accept_mode(pn_messenger_t *messenger, pn_accept_mode_t mode);

int pn_messenger_get_outgoing_window(pn_messenger_t *messenger);

int pn_messenger_set_outgoing_window(pn_messenger_t *messenger, int window);

int pn_messenger_get_incoming_window(pn_messenger_t *messenger);

int pn_messenger_set_incoming_window(pn_messenger_t *messenger, int window);

/** Starts a messenger. A messenger cannot send or recv messages until
 * it is started.
 *
 * @param[in] messenger the messenger to start
 *
 * @return an error code or zero on success
 * @see error.h
 */
int pn_messenger_start(pn_messenger_t *messenger);

/** Stops a messenger. A messenger cannot send or recv messages when
 *  it is stopped.
 *
 * @param[in] messenger the messenger to stop
 *
 * @return an error code or zero on success
 * @see error.h
 */
int pn_messenger_stop(pn_messenger_t *messenger);

/** Subscribes a messenger to messages from the specified source.
 *
 * @param[in] messenger the messenger to subscribe
 * @param[in] source
 *
 * @return a subscription
 */
pn_subscription_t *pn_messenger_subscribe(pn_messenger_t *messenger, const char *source);

void *pn_subscription_get_context(pn_subscription_t *sub);

void pn_subscription_set_context(pn_subscription_t *sub, void *context);

/** Puts a message on the outgoing message queue for a messenger.
 *
 * @param[in] messenger the messenger
 * @param[in] msg the message to put on the outgoing queue
 *
 * @return an error code or zero on success
 * @see error.h
 */
int pn_messenger_put(pn_messenger_t *messenger, pn_message_t *msg);

/** Gets the last known remote state of the delivery associated with
 * the given tracker.
 *
 * @param[in] messenger the messenger
 * @param[in] tracker the tracker identify the delivery
 *
 * @return a status code for the delivery
 */
pn_status_t pn_messenger_status(pn_messenger_t *messenger, pn_tracker_t tracker);

int pn_messenger_settle(pn_messenger_t *messenger, pn_tracker_t tracker, int flags);

/** Gets the tracker for the message most recently provided to
 * pn_messenger_put.
 *
 * @param[in] messenger the messenger
 *
 * @return a pn_tracker_t or an undefined value if pn_messenger_get
 *         has never been called for the given messenger
 */
pn_tracker_t pn_messenger_outgoing_tracker(pn_messenger_t *messenger);

/** Sends any messages in the outgoing message queue for a messenger.
 * This will block until the messages have been sent.
 *
 * @param[in] messenger the messager
 *
 * @return an error code or zero on success
 * @see error.h
 */
int pn_messenger_send(pn_messenger_t *messenger);

/** Receives up to n message into the incoming message queue of a
 * messenger. Blocks until at least one message is available in the
 * incoming queue.
 *
 * @param[in] messenger the messenger
 * @param[in] n the maximum number of messages to receive
 *
 * @return an error code or zero on success
 * @see error.h
 */
int pn_messenger_recv(pn_messenger_t *messenger, int n);

/** Gets a message from the head of the incoming message queue of a
 * messenger.
 *
 * @param[in] messenger the messenger
 * @param[out] msg upon return contains the message from the head of the queue
 *
 * @return an error code or zero on success
 * @see error.h
 */
int pn_messenger_get(pn_messenger_t *messenger, pn_message_t *msg);

/** Gets the tracker for the message most recently fetched by
 * pn_messenger_get.
 *
 * @param[in] messenger the messenger
 *
 * @return a pn_tracker_t or an undefined value if pn_messenger_get
 *         has never been called for the given messenger
 */
pn_tracker_t pn_messenger_incoming_tracker(pn_messenger_t *messenger);

pn_subscription_t *pn_messenger_incoming_subscription(pn_messenger_t *messenger);

#define PN_CUMULATIVE (0x1)

/** Accepts the incoming messages identified by the tracker. Use the
 * PN_CUMULATIVE flag to accept everything prior to the supplied
 * tracker.
 *
 * @param[in] messenger the messenger
 *
 * @return an error code or zero on success
 * @see error.h
 */
int pn_messenger_accept(pn_messenger_t *messenger, pn_tracker_t tracker, int flags);

/** Rejects the incoming messages identified by the tracker. Use the
 * PN_CUMULATIVE flag to reject everything prior to the supplied
 * tracker.
 *
 * @param[in] messenger the messenger
 *
 * @return an error code or zero on success
 * @see error.h
 */
int pn_messenger_reject(pn_messenger_t *messenger, pn_tracker_t tracker, int flags);

/** Returns the number of messages in the outgoing message queue of a messenger.
 *
 * @param[in] the messenger
 *
 * @return the outgoing queue depth
 */
int pn_messenger_outgoing(pn_messenger_t *messenger);

/** Returns the number of messages in the incoming message queue of a messenger.
 *
 * @param[in] the messenger
 *
 * @return the incoming queue depth
 */
int pn_messenger_incoming(pn_messenger_t *messenger);

#ifdef __cplusplus
}
#endif

#endif /* messenger.h */
