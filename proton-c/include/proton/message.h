#ifndef PROTON_MESSAGE_H
#define PROTON_MESSAGE_H 1

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

#include <proton/types.h>
#include <proton/codec.h>
#include <sys/types.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pn_message_t pn_message_t;

typedef enum {
  PN_DATA,
  PN_TEXT,
  PN_AMQP,
  PN_JSON
} pn_format_t;

#define PN_DEFAULT_PRIORITY (4)

pn_message_t * pn_message();
void           pn_message_free(pn_message_t *msg);

void           pn_message_clear(pn_message_t *msg);
int            pn_message_errno(pn_message_t *msg);
const char *   pn_message_error(pn_message_t *msg);

bool           pn_message_is_inferred(pn_message_t *msg);
int            pn_message_set_inferred(pn_message_t *msg, bool inferred);

// standard message headers and properties
bool           pn_message_is_durable            (pn_message_t *msg);
int            pn_message_set_durable           (pn_message_t *msg, bool durable);

uint8_t        pn_message_get_priority          (pn_message_t *msg);
int            pn_message_set_priority          (pn_message_t *msg, uint8_t priority);

pn_millis_t    pn_message_get_ttl               (pn_message_t *msg);
int            pn_message_set_ttl               (pn_message_t *msg, pn_millis_t ttl);

bool           pn_message_is_first_acquirer     (pn_message_t *msg);
int            pn_message_set_first_acquirer    (pn_message_t *msg, bool first);

uint32_t       pn_message_get_delivery_count    (pn_message_t *msg);
int            pn_message_set_delivery_count    (pn_message_t *msg, uint32_t count);

pn_data_t *    pn_message_id                    (pn_message_t *msg);
pn_atom_t      pn_message_get_id                (pn_message_t *msg);
int            pn_message_set_id                (pn_message_t *msg, pn_atom_t id);

pn_bytes_t     pn_message_get_user_id           (pn_message_t *msg);
int            pn_message_set_user_id           (pn_message_t *msg, pn_bytes_t user_id);

const char *   pn_message_get_address           (pn_message_t *msg);
int            pn_message_set_address           (pn_message_t *msg, const char *address);

const char *   pn_message_get_subject           (pn_message_t *msg);
int            pn_message_set_subject           (pn_message_t *msg, const char *subject);

const char *   pn_message_get_reply_to          (pn_message_t *msg);
int            pn_message_set_reply_to          (pn_message_t *msg, const char *reply_to);

pn_data_t *    pn_message_correlation_id        (pn_message_t *msg);
pn_atom_t      pn_message_get_correlation_id    (pn_message_t *msg);
int            pn_message_set_correlation_id    (pn_message_t *msg, pn_atom_t atom);

const char *   pn_message_get_content_type      (pn_message_t *msg);
int            pn_message_set_content_type      (pn_message_t *msg, const char *type);

const char *   pn_message_get_content_encoding  (pn_message_t *msg);
int            pn_message_set_content_encoding  (pn_message_t *msg, const char *encoding);

pn_timestamp_t pn_message_get_expiry_time       (pn_message_t *msg);
int            pn_message_set_expiry_time       (pn_message_t *msg, pn_timestamp_t time);

pn_timestamp_t pn_message_get_creation_time     (pn_message_t *msg);
int            pn_message_set_creation_time     (pn_message_t *msg, pn_timestamp_t time);

const char *   pn_message_get_group_id          (pn_message_t *msg);
int            pn_message_set_group_id          (pn_message_t *msg, const char *group_id);

pn_sequence_t  pn_message_get_group_sequence    (pn_message_t *msg);
int            pn_message_set_group_sequence    (pn_message_t *msg, pn_sequence_t n);

const char *   pn_message_get_reply_to_group_id (pn_message_t *msg);
int            pn_message_set_reply_to_group_id (pn_message_t *msg, const char *reply_to_group_id);

pn_format_t pn_message_get_format(pn_message_t *message);
int pn_message_set_format(pn_message_t *message, pn_format_t format);

int pn_message_load(pn_message_t *message, const char *data, size_t size);
int pn_message_load_data(pn_message_t *message, const char *data, size_t size);
int pn_message_load_text(pn_message_t *message, const char *data, size_t size);
int pn_message_load_amqp(pn_message_t *message, const char *data, size_t size);
int pn_message_load_json(pn_message_t *message, const char *data, size_t size);

int pn_message_save(pn_message_t *message, char *data, size_t *size);
int pn_message_save_data(pn_message_t *message, char *data, size_t *size);
int pn_message_save_text(pn_message_t *message, char *data, size_t *size);
int pn_message_save_amqp(pn_message_t *message, char *data, size_t *size);
int pn_message_save_json(pn_message_t *message, char *data, size_t *size);

pn_data_t *pn_message_instructions(pn_message_t *msg);
pn_data_t *pn_message_annotations(pn_message_t *msg);
pn_data_t *pn_message_properties(pn_message_t *msg);
pn_data_t *pn_message_body(pn_message_t *msg);

int pn_message_decode(pn_message_t *msg, const char *bytes, size_t size);
int pn_message_encode(pn_message_t *msg, char *bytes, size_t *size);

ssize_t pn_message_data(char *dst, size_t available, const char *src, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* message.h */
