#ifndef _PROTON_MESSENGER_H
#define _PROTON_MESSENGER_H 1

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

/** @file
 * The messenger API provides a high level interface for sending and
 * receiving AMQP messages.
 */

typedef struct pn_messenger_t pn_messenger_t; /**< Messenger*/

/** Constructs a new Messenger.
 *
 * @return pointer to a new Messenger
 */
pn_messenger_t *pn_messenger();

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
 * @return an error code or zero on success
 * @see error.h
 */
int pn_messenger_subscribe(pn_messenger_t *messenger, const char *source);

/** Puts a message on the outgoing message queue for a messenger.
 *
 * @param[in] messenger the messenger
 * @param[in] msg the message to put on the outgoing queue
 *
 * @return an error code or zero on success
 * @see error.h
 */
int pn_messenger_put(pn_messenger_t *messenger, pn_message_t *msg);

/** Sends any messages in the outgoing message queue for a messenger.
 * This will blocks until the messages have been sent.
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

#endif /* messenger.h */
