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

typedef struct pn_messenger_t pn_messenger_t;
typedef int pn_source_t;
typedef int pn_target_t;

pn_messenger_t *pn_messenger();
void pn_messenger_free(pn_messenger_t *messenger);

const char *pn_messenger_error(pn_messenger_t *messenger);

int pn_messenger_start(pn_messenger_t *messenger);
int pn_messenger_stop(pn_messenger_t *messenger);

int pn_messenger_subscribe(pn_messenger_t *messenger, const char *source);

int pn_messenger_put(pn_messenger_t *messenger, pn_message_t *msg);
int pn_messenger_send(pn_messenger_t *messenger);

int pn_messenger_recv(pn_messenger_t *messenger, int n);
int pn_messenger_get(pn_messenger_t *messenger, pn_message_t *msg);

int pn_messenger_outgoing(pn_messenger_t *messenger);
int pn_messenger_incoming(pn_messenger_t *messenger);

#endif /* messenger.h */
