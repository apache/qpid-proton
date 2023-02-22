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

#include "platform/platform_fmt.h"

#include "data.h"
#include "encodings.h"
#include "max_align.h"
#include "message-internal.h"
#include "protocol.h"
#include "util.h"

#include "core/fixed_string.h"
#include "core/frame_generators.h"
#include "core/frame_consumers.h"

#include <proton/link.h>
#include <proton/object.h>
#include <proton/codec.h>
#include <proton/error.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

// message

struct pn_message_t {
  pn_atom_t id;
  pn_atom_t correlation_id;
  pn_bytes_t instructions_raw;
  pn_bytes_t annotations_raw;
  pn_bytes_t properties_raw;
  pn_bytes_t body_raw;
  pn_timestamp_t expiry_time;
  pn_timestamp_t creation_time;
  pn_string_t *user_id;
  pn_string_t *address;
  pn_string_t *subject;
  pn_string_t *reply_to;
  pn_string_t *content_type;
  pn_string_t *content_encoding;
  pn_string_t *group_id;
  pn_string_t *reply_to_group_id;

  pn_data_t *id_deprecated;
  pn_data_t *correlation_id_deprecated;
  pn_data_t *instructions_deprecated;
  pn_data_t *annotations_deprecated;
  pn_data_t *properties_deprecated;
  pn_data_t *body_deprecated;
  pn_error_t *error;

  pn_sequence_t group_sequence;
  pn_millis_t ttl;
  uint32_t delivery_count;

  uint8_t priority;

  bool durable;
  bool first_acquirer;
  bool inferred;
};

void pni_msgid_clear(pn_atom_t* msgid) {
  switch (msgid->type) {
    case PN_BINARY:
    case PN_STRING:
      free((void*)msgid->u.as_bytes.start);
      PN_FALLTHROUGH;
    case PN_ULONG:
    case PN_UUID:
      msgid->type = PN_NULL;
      PN_FALLTHROUGH;
    case PN_NULL:
      return;
    default:
      break;
  }
  assert(false);
}

void pni_msgid_validate_intern(pn_atom_t* msgid) {
  switch (msgid->type) {
    case PN_BINARY:
    case PN_STRING: {
      char* new = malloc(msgid->u.as_bytes.size);
      assert(new);
      memcpy(new, msgid->u.as_bytes.start, msgid->u.as_bytes.size);
      msgid->u.as_bytes.start = new;
      return;
    }
    case PN_ULONG:
    case PN_UUID:
    case PN_NULL:
      return;
    default:
      // Not a legal msgid type
      msgid->type = PN_NULL;
      return;
  }
}

/* This exists purely to fix bad incoming ids created by the broken ruby binding */
void pni_msgid_fix_interop(pn_atom_t* msgid) {
  switch (msgid->type) {
    case PN_INT: {
      int32_t v = msgid->u.as_int;
      // Only fix if the value actually is positive
      if (v < 0) return;
      msgid->type = PN_ULONG;
      msgid->u.as_ulong = v;
      return;
    }
    case PN_LONG: {
      int64_t v = msgid->u.as_long;
      // Only fix if the value actually is positive
      if (v < 0) return;
      msgid->type = PN_ULONG;
      msgid->u.as_ulong = v;
      return;
    }
    default:
      return;
  }
}

void pn_message_finalize(void *obj)
{
  pn_message_t *msg = (pn_message_t *) obj;
  pn_free(msg->user_id);
  pn_free(msg->address);
  pn_free(msg->subject);
  pn_free(msg->reply_to);
  pn_free(msg->content_type);
  pn_free(msg->content_encoding);
  pn_free(msg->group_id);
  pn_free(msg->reply_to_group_id);
  pni_msgid_clear(&msg->id);
  pni_msgid_clear(&msg->correlation_id);
  pn_bytes_free(msg->instructions_raw);
  pn_bytes_free(msg->annotations_raw);
  pn_bytes_free(msg->properties_raw);
  pn_bytes_free(msg->body_raw);
  pn_data_free(msg->id_deprecated);
  pn_data_free(msg->correlation_id_deprecated);
  pn_data_free(msg->instructions_deprecated);
  pn_data_free(msg->annotations_deprecated);
  pn_data_free(msg->properties_deprecated);
  pn_data_free(msg->body_deprecated);
  pn_error_free(msg->error);
}

void pn_message_inspect(void *obj, pn_fixed_string_t *dst)
{
  pn_message_t *msg = (pn_message_t *) obj;
  pn_fixed_string_addf(dst, "Message{");

  bool comma = false;

  if (pn_string_get(msg->address)) {
    pn_fixed_string_addf(dst, "address=");
    pn_finspect(msg->address, dst);
    pn_fixed_string_addf(dst, ", ");
    comma = true;
  }

  if (msg->durable) {
    pn_fixed_string_addf(dst, "durable=%i, ", msg->durable);
    comma = true;
  }

  if (msg->priority != HEADER_PRIORITY_DEFAULT) {
    pn_fixed_string_addf(dst, "priority=%u, ", msg->priority);
    comma = true;
  }

  if (msg->ttl) {
    pn_fixed_string_addf(dst, "ttl=%" PRIu32 ", ", msg->ttl);
    comma = true;
  }

  if (msg->first_acquirer) {
    pn_fixed_string_addf(dst, "first_acquirer=%i, ", msg->first_acquirer);
    comma = true;
  }

  if (msg->delivery_count) {
    pn_fixed_string_addf(dst, "delivery_count=%" PRIu32 ", ", msg->delivery_count);
    comma = true;
  }

  pn_atom_t id = pn_message_get_id(msg);
  if (id.type!=PN_NULL) {
    pn_fixed_string_addf(dst, "id=");
    pni_inspect_atom(&id, dst);
    pn_fixed_string_addf(dst, ", ");
    comma = true;
  }

  if (pn_string_get(msg->user_id)) {
    pn_fixed_string_addf(dst, "user_id=");
    pn_finspect(msg->user_id, dst);
    pn_fixed_string_addf(dst, ", ");
    comma = true;
  }

  if (pn_string_get(msg->subject)) {
    pn_fixed_string_addf(dst, "subject=");
    pn_finspect(msg->subject, dst);
    pn_fixed_string_addf(dst, ", ");
    comma = true;
  }

  if (pn_string_get(msg->reply_to)) {
    pn_fixed_string_addf(dst, "reply_to=");
    pn_finspect(msg->reply_to, dst);
    pn_fixed_string_addf(dst, ", ");
    comma = true;
  }

  pn_atom_t correlation_id = pn_message_get_correlation_id(msg);
  if (correlation_id.type!=PN_NULL) {
    pn_fixed_string_addf(dst, "correlation_id=");
    pni_inspect_atom(&correlation_id, dst);
    pn_fixed_string_addf(dst, ", ");
    comma = true;
  }

  if (pn_string_get(msg->content_type)) {
    pn_fixed_string_addf(dst, "content_type=");
    pn_finspect(msg->content_type, dst);
    pn_fixed_string_addf(dst, ", ");
    comma = true;
  }

  if (pn_string_get(msg->content_encoding)) {
    pn_fixed_string_addf(dst, "content_encoding=");
    pn_finspect(msg->content_encoding, dst);
    pn_fixed_string_addf(dst, ", ");
    comma = true;
  }

  if (msg->expiry_time) {
    pn_fixed_string_addf(dst, "expiry_time=%" PRIi64 ", ", msg->expiry_time);
    comma = true;
  }

  if (msg->creation_time) {
    pn_fixed_string_addf(dst, "creation_time=%" PRIi64 ", ", msg->creation_time);
    comma = true;
  }

  if (pn_string_get(msg->group_id)) {
    pn_fixed_string_addf(dst, "group_id=");
    pn_finspect(msg->group_id, dst);
    pn_fixed_string_addf(dst, ", ");
    comma = true;
  }

  if (msg->group_sequence) {
    pn_fixed_string_addf(dst, "group_sequence=%" PRIu32 ", ", msg->group_sequence);
    comma = true;
  }

  if (pn_string_get(msg->reply_to_group_id)) {
    pn_fixed_string_addf(dst, "reply_to_group_id=");
    pn_finspect(msg->reply_to_group_id, dst);
    pn_fixed_string_addf(dst, ", ");
    comma = true;
  }

  if (msg->inferred) {
    pn_fixed_string_addf(dst, "inferred=%i, ", msg->inferred);
    comma = true;
  }

  if (pn_data_size(msg->instructions_deprecated)) {
    pn_fixed_string_addf(dst, "instructions=");
    pn_finspect(msg->instructions_deprecated, dst);
    pn_fixed_string_addf(dst, ", ");
    comma = true;
  }

  if (pn_data_size(msg->annotations_deprecated)) {
    pn_fixed_string_addf(dst, "annotations=");
    pn_finspect(msg->annotations_deprecated, dst);
    pn_fixed_string_addf(dst, ", ");
    comma = true;
  }

  if (pn_data_size(msg->properties_deprecated)) {
    pn_fixed_string_addf(dst, "properties=");
    pn_finspect(msg->properties_deprecated, dst);
    pn_fixed_string_addf(dst, ", ");
    comma = true;
  }

  if (pn_data_size(msg->body_deprecated)) {
    pn_fixed_string_addf(dst, "body=");
    pn_finspect(msg->body_deprecated, dst);
    pn_fixed_string_addf(dst, ", ");
    comma = true;
  }

  if (comma) {
    dst->position = dst->position - 2;
 }

  pn_fixed_string_addf(dst, "}");
  return;
}

#define pn_message_initialize NULL
#define pn_message_hashcode NULL
#define pn_message_compare NULL

static pn_message_t *pni_message_new(size_t size)
{
  static const pn_class_t clazz = PN_CLASS(pn_message);
  pn_message_t *msg = (pn_message_t *) pn_class_new(&clazz, size);
  msg->durable = false;
  msg->priority = HEADER_PRIORITY_DEFAULT;
  msg->ttl = 0;
  msg->first_acquirer = false;
  msg->delivery_count = 0;
  msg->id = (pn_atom_t){.type=PN_NULL};
  msg->user_id = pn_string(NULL);
  msg->address = pn_string(NULL);
  msg->subject = pn_string(NULL);
  msg->reply_to = pn_string(NULL);
  msg->correlation_id = (pn_atom_t){.type=PN_NULL};
  msg->content_type = pn_string(NULL);
  msg->content_encoding = pn_string(NULL);
  msg->expiry_time = 0;
  msg->creation_time = 0;
  msg->group_id = pn_string(NULL);
  msg->group_sequence = 0;
  msg->reply_to_group_id = pn_string(NULL);
  msg->instructions_raw = (pn_bytes_t){0, 0};
  msg->annotations_raw = (pn_bytes_t){0, 0};
  msg->properties_raw = (pn_bytes_t){0, 0};
  msg->body_raw = (pn_bytes_t){0, 0};

  msg->inferred = false;
  msg->id_deprecated = NULL;
  msg->correlation_id_deprecated = NULL;
  msg->instructions_deprecated = NULL;
  msg->annotations_deprecated = NULL;
  msg->properties_deprecated = NULL;
  msg->body_deprecated = NULL;

  msg->error = pn_error();
  return msg;
}

pn_message_t *pn_message(void) {
  return pni_message_new(sizeof(pn_message_t));
}

/* Maximally aligned message to make extra storage safe for any type */
typedef union {
  pn_message_t m;
  pn_max_align_t a;
}  pni_aligned_message_t;

pn_message_t *pni_message_with_extra(size_t extra) {
  return pni_message_new(sizeof(pni_aligned_message_t) + extra);
}

void *pni_message_get_extra(pn_message_t *m) {
  return ((char*)m) + sizeof(pni_aligned_message_t);
}

void pn_message_free(pn_message_t *msg)
{
  pn_free(msg);
}

void pn_message_clear(pn_message_t *msg)
{
  msg->durable = false;
  msg->priority = HEADER_PRIORITY_DEFAULT;
  msg->ttl = 0;
  msg->first_acquirer = false;
  msg->delivery_count = 0;
  pni_msgid_clear(&msg->id);
  pn_string_clear(msg->user_id);
  pn_string_clear(msg->address);
  pn_string_clear(msg->subject);
  pn_string_clear(msg->reply_to);
  pni_msgid_clear(&msg->correlation_id);
  pn_string_clear(msg->content_type);
  pn_string_clear(msg->content_encoding);
  msg->expiry_time = 0;
  msg->creation_time = 0;
  pn_string_clear(msg->group_id);
  msg->group_sequence = 0;
  pn_string_clear(msg->reply_to_group_id);
  msg->inferred = false;
  pn_bytes_free(msg->annotations_raw);
  pn_bytes_free(msg->instructions_raw);
  pn_bytes_free(msg->properties_raw);
  pn_bytes_free(msg->body_raw);
  msg->instructions_raw = (pn_bytes_t){0, NULL};
  msg->annotations_raw = (pn_bytes_t){0, NULL};
  msg->properties_raw = (pn_bytes_t){0, NULL};
  msg->body_raw = (pn_bytes_t){0, NULL};
  pn_data_clear(msg->id_deprecated);
  pn_data_clear(msg->correlation_id_deprecated);
  pn_data_clear(msg->instructions_deprecated);
  pn_data_clear(msg->annotations_deprecated);
  pn_data_clear(msg->properties_deprecated);
  pn_data_clear(msg->body_deprecated);
}

int pn_message_errno(pn_message_t *msg)
{
  assert(msg);
  return pn_error_code(msg->error);
}

pn_error_t *pn_message_error(pn_message_t *msg)
{
  assert(msg);
  return msg->error;
}

bool pn_message_is_inferred(pn_message_t *msg)
{
  assert(msg);
  return msg->inferred;
}

int pn_message_set_inferred(pn_message_t *msg, bool inferred)
{
  assert(msg);
  // If the inferred value changed and we're only holding the raw bytes then we need to get the
  // deprecated pn_data_t equivalent as the raw bytes must be reconstructed from the pn_data_t
  // interpreted in light of the inferred flag.
  if (msg->inferred!=inferred && msg->body_raw.size>0) {
    (void) pn_message_body(msg);
  }
  msg->inferred = inferred;
  return 0;
}

bool pn_message_is_durable(pn_message_t *msg)
{
  assert(msg);
  return msg->durable;
}
int pn_message_set_durable(pn_message_t *msg, bool durable)
{
  assert(msg);
  msg->durable = durable;
  return 0;
}


uint8_t pn_message_get_priority(pn_message_t *msg)
{
  assert(msg);
  return msg->priority;
}
int pn_message_set_priority(pn_message_t *msg, uint8_t priority)
{
  assert(msg);
  msg->priority = priority;
  return 0;
}

pn_millis_t pn_message_get_ttl(pn_message_t *msg)
{
  assert(msg);
  return msg->ttl;
}
int pn_message_set_ttl(pn_message_t *msg, pn_millis_t ttl)
{
  assert(msg);
  msg->ttl = ttl;
  return 0;
}

bool pn_message_is_first_acquirer(pn_message_t *msg)
{
  assert(msg);
  return msg->first_acquirer;
}
int pn_message_set_first_acquirer(pn_message_t *msg, bool first)
{
  assert(msg);
  msg->first_acquirer = first;
  return 0;
}

uint32_t pn_message_get_delivery_count(pn_message_t *msg)
{
  assert(msg);
  return msg->delivery_count;
}
int pn_message_set_delivery_count(pn_message_t *msg, uint32_t count)
{
  assert(msg);
  msg->delivery_count = count;
  return 0;
}

pn_data_t *pn_message_id(pn_message_t *msg)
{
  assert(msg);
  if (!msg->id_deprecated) {
    msg->id_deprecated = pn_data(1);
    if (msg->id.type!=PN_NULL) {
      pn_data_put_atom(msg->id_deprecated, msg->id);
      pni_msgid_clear(&msg->id);
    }
  }
  return msg->id_deprecated;
}

pn_msgid_t pn_message_get_id(pn_message_t *msg)
{
  assert(msg);
  if (msg->id_deprecated) {
    return pn_data_get_atom(msg->id_deprecated);
  } else {
    return msg->id;
  }
}

int pn_message_set_id(pn_message_t *msg, pn_msgid_t id)
{
  assert(msg);
  if (msg->id_deprecated) {
    pn_data_rewind(msg->id_deprecated);
    pn_data_put_atom(msg->id_deprecated, id);
  } else {
    pni_msgid_clear(&msg->id);
    msg->id = id;
    pni_msgid_validate_intern(&msg->id);
  }
  return 0;
}

static int pn_string_set_bytes(pn_string_t *string, pn_bytes_t bytes)
{
  return pn_string_setn(string, bytes.start, bytes.size);
}

pn_bytes_t pn_message_get_user_id(pn_message_t *msg)
{
  assert(msg);
  return pn_string_bytes(msg->user_id);
}
int pn_message_set_user_id(pn_message_t *msg, pn_bytes_t user_id)
{
  assert(msg);
  return pn_string_set_bytes(msg->user_id, user_id);
}

const char *pn_message_get_address(pn_message_t *msg)
{
  assert(msg);
  return pn_string_get(msg->address);
}
int pn_message_set_address(pn_message_t *msg, const char *address)
{
  assert(msg);
  return pn_string_set(msg->address, address);
}

const char *pn_message_get_subject(pn_message_t *msg)
{
  assert(msg);
  return pn_string_get(msg->subject);
}
int pn_message_set_subject(pn_message_t *msg, const char *subject)
{
  assert(msg);
  return pn_string_set(msg->subject, subject);
}

const char *pn_message_get_reply_to(pn_message_t *msg)
{
  assert(msg);
  return pn_string_get(msg->reply_to);
}
int pn_message_set_reply_to(pn_message_t *msg, const char *reply_to)
{
  assert(msg);
  return pn_string_set(msg->reply_to, reply_to);
}

pn_data_t *pn_message_correlation_id(pn_message_t *msg)
{
  assert(msg);
  if (!msg->correlation_id_deprecated) {
    msg->correlation_id_deprecated = pn_data(1);
    if (msg->correlation_id.type!=PN_NULL) {
      pn_data_put_atom(msg->correlation_id_deprecated, msg->correlation_id);
      pni_msgid_clear(&msg->correlation_id);
    }
  }
  return msg->correlation_id_deprecated;
}

pn_msgid_t pn_message_get_correlation_id(pn_message_t *msg)
{
  assert(msg);
  if (msg->correlation_id_deprecated) {
    return pn_data_get_atom(msg->correlation_id_deprecated);
  } else {
    return msg->correlation_id;
  }
}

int pn_message_set_correlation_id(pn_message_t *msg, pn_msgid_t id)
{
  assert(msg);
  if (msg->correlation_id_deprecated) {
    pn_data_rewind(msg->correlation_id_deprecated);
    pn_data_put_atom(msg->correlation_id_deprecated, id);
  } else {
    pni_msgid_clear(&msg->correlation_id);
    msg->correlation_id = id;
    pni_msgid_validate_intern(&msg->correlation_id);
  }
  return 0;
}


const char *pn_message_get_content_type(pn_message_t *msg)
{
  assert(msg);
  return pn_string_get(msg->content_type);
}
int pn_message_set_content_type(pn_message_t *msg, const char *type)
{
  assert(msg);
  return pn_string_set(msg->content_type, type);
}

const char *pn_message_get_content_encoding(pn_message_t *msg)
{
  assert(msg);
  return pn_string_get(msg->content_encoding);
}
int pn_message_set_content_encoding(pn_message_t *msg, const char *encoding)
{
  assert(msg);
  return pn_string_set(msg->content_encoding, encoding);
}

pn_timestamp_t pn_message_get_expiry_time(pn_message_t *msg)
{
  assert(msg);
  return msg->expiry_time;
}
int pn_message_set_expiry_time(pn_message_t *msg, pn_timestamp_t time)
{
  assert(msg);
  msg->expiry_time = time;
  return 0;
}

pn_timestamp_t pn_message_get_creation_time(pn_message_t *msg)
{
  assert(msg);
  return msg->creation_time;
}
int pn_message_set_creation_time(pn_message_t *msg, pn_timestamp_t time)
{
  assert(msg);
  msg->creation_time = time;
  return 0;
}

const char *pn_message_get_group_id(pn_message_t *msg)
{
  assert(msg);
  return pn_string_get(msg->group_id);
}
int pn_message_set_group_id(pn_message_t *msg, const char *group_id)
{
  assert(msg);
  return pn_string_set(msg->group_id, group_id);
}

pn_sequence_t pn_message_get_group_sequence(pn_message_t *msg)
{
  assert(msg);
  return msg->group_sequence;
}
int pn_message_set_group_sequence(pn_message_t *msg, pn_sequence_t n)
{
  assert(msg);
  msg->group_sequence = n;
  return 0;
}

const char *pn_message_get_reply_to_group_id(pn_message_t *msg)
{
  assert(msg);
  return pn_string_get(msg->reply_to_group_id);
}
int pn_message_set_reply_to_group_id(pn_message_t *msg, const char *reply_to_group_id)
{
  assert(msg);
  return pn_string_set(msg->reply_to_group_id, reply_to_group_id);
}

int pn_message_decode(pn_message_t *msg, const char *bytes, size_t size)
{
  assert(msg && bytes && size);

  pn_bytes_t msg_bytes = {.size=size, .start=bytes};
  pn_bytes_t instructions_bytes = {0, 0};
  pn_bytes_t annotations_bytes = {0, 0};
  pn_bytes_t properties_bytes = {0, 0};
  pn_bytes_t body_bytes = {0, 0};
  pn_bytes_t unknown_section_bytes = {0, 0};

  while (msg_bytes.size) {
    bool scanned;
    uint64_t desc;
    size_t section_size = pn_amqp_decode_DQLq(msg_bytes, &scanned, &desc);
    if (!scanned) {
      desc = 0;
    }

    switch (desc) {
      case HEADER: {
        bool priority_q;
        uint8_t priority;
        pn_amqp_decode_DqEoQBIoIe(msg_bytes,
                                  &msg->durable,
                                  &priority_q, &priority,
                                  &msg->ttl,
                                  &msg->first_acquirer,
                                  &msg->delivery_count);
        msg->priority = priority_q ? priority : HEADER_PRIORITY_DEFAULT;
        break;
      }
      case PROPERTIES: {
        pn_bytes_t user_id, address, subject, reply_to, ctype, cencoding,
                   group_id, reply_to_group_id;
        pn_atom_t id;
        pn_atom_t correlation_id;
        pn_amqp_decode_DqEazSSSassttSISe(msg_bytes,  &id,
                           &user_id, &address, &subject, &reply_to,
                           &correlation_id, &ctype, &cencoding,
                           &msg->expiry_time, &msg->creation_time, &group_id,
                           &msg->group_sequence, &reply_to_group_id);
        pni_msgid_fix_interop(&id);
        pn_message_set_id(msg, id);
        int err = pn_string_set_bytes(msg->user_id, user_id);
        if (err) return pn_error_format(msg->error, err, "error setting user_id");
        err = pn_string_setn(msg->address, address.start, address.size);
        if (err) return pn_error_format(msg->error, err, "error setting address");
        err = pn_string_setn(msg->subject, subject.start, subject.size);
        if (err) return pn_error_format(msg->error, err, "error setting subject");
        err = pn_string_setn(msg->reply_to, reply_to.start, reply_to.size);
        if (err) return pn_error_format(msg->error, err, "error setting reply_to");
        pni_msgid_fix_interop(&correlation_id);
        pn_message_set_correlation_id(msg, correlation_id);
        err = pn_string_setn(msg->content_type, ctype.start, ctype.size);
        if (err) return pn_error_format(msg->error, err, "error setting content_type");
        err = pn_string_setn(msg->content_encoding, cencoding.start,
                             cencoding.size);
        if (err) return pn_error_format(msg->error, err, "error setting content_encoding");
        err = pn_string_setn(msg->group_id, group_id.start, group_id.size);
        if (err) return pn_error_format(msg->error, err, "error setting group_id");
        err = pn_string_setn(msg->reply_to_group_id, reply_to_group_id.start,
                             reply_to_group_id.size);
        if (err) return pn_error_format(msg->error, err, "error setting reply_to_group_id");
        break;
      }
      case DELIVERY_ANNOTATIONS: {
        pn_amqp_decode_DqR(msg_bytes, &instructions_bytes);
        break;
      }
      case MESSAGE_ANNOTATIONS: {
        pn_amqp_decode_DqR(msg_bytes, &annotations_bytes);
        break;
      }
      case APPLICATION_PROPERTIES: {
        pn_amqp_decode_DqR(msg_bytes, &properties_bytes);
        break;
      }
      case DATA:
      case AMQP_SEQUENCE: {
        msg->inferred = true;
        pn_amqp_decode_DqR(msg_bytes, &body_bytes);
        break;
      }
      case AMQP_VALUE: {
        msg->inferred = false;
        pn_amqp_decode_DqR(msg_bytes, &body_bytes);
        break;
      }
      case FOOTER:
        break;
      default: {
        pn_amqp_decode_R(msg_bytes, &unknown_section_bytes);
        break;
      }
    }
    msg_bytes = (pn_bytes_t){.size=msg_bytes.size-section_size, .start=msg_bytes.start+section_size};
  }
  pn_bytes_free(msg->instructions_raw);
  msg->instructions_raw = pn_bytes_dup(instructions_bytes);
  pn_bytes_free(msg->annotations_raw);
  msg->annotations_raw = pn_bytes_dup(annotations_bytes);
  pn_bytes_free(msg->properties_raw);
  msg->properties_raw = pn_bytes_dup(properties_bytes);
  pn_bytes_free(msg->body_raw);
  msg->body_raw = pn_bytes_dup(body_bytes);
  return 0;
}

int pn_message_encode(pn_message_t *msg, char *bytes, size_t *isize)
{
  pn_rwbytes_t scratch = (pn_rwbytes_t){.size=*isize, .start=bytes};
  if (!pni_switch_to_raw_bytes(scratch, &msg->instructions_deprecated, &msg->instructions_raw)) {
    return PN_OVERFLOW;
  }
  if (!pni_switch_to_raw_bytes(scratch, &msg->annotations_deprecated, &msg->annotations_raw)) {
    return PN_OVERFLOW;
  }
  if (!pni_switch_to_raw_bytes(scratch, &msg->properties_deprecated, &msg->properties_raw)) {
    return PN_OVERFLOW;
  }
  if (!pni_switch_to_raw_bytes(scratch, &msg->body_deprecated, &msg->body_raw)) {
    return PN_OVERFLOW;
  }
  size_t remaining = *isize;
  size_t total = 0;

  /* "DL[?o?B?I?o?I]" */
  size_t last_size = pn_amqp_encode_bytes_DLEQoQBQIQoQIe(bytes, remaining, HEADER,
                        msg->durable, msg->durable,
                         msg->priority!=HEADER_PRIORITY_DEFAULT, msg->priority,
                         (bool)msg->ttl, msg->ttl,
                         msg->first_acquirer, msg->first_acquirer,
                         (bool)msg->delivery_count, msg->delivery_count);
  if (last_size > remaining) return PN_OVERFLOW;

  remaining -= last_size;
  bytes += last_size;
  total += last_size;


  if (msg->instructions_raw.size>0) {
    last_size = pn_amqp_encode_bytes_DLR(bytes, remaining, DELIVERY_ANNOTATIONS, msg->instructions_raw);
    if (last_size > remaining) return PN_OVERFLOW;

    remaining -= last_size;
    bytes += last_size;
    total += last_size;
  }

  if (msg->annotations_raw.size>0) {
    last_size = pn_amqp_encode_bytes_DLR(bytes, remaining, MESSAGE_ANNOTATIONS, msg->annotations_raw);
    if (last_size > remaining) return PN_OVERFLOW;

    remaining -= last_size;
    bytes += last_size;
    total += last_size;
  }

  /* "DL[CzSSSCss?t?tS?IS]" */
  pn_atom_t id = pn_message_get_id(msg);
  pn_atom_t correlation_id = pn_message_get_correlation_id(msg);
  last_size = pn_amqp_encode_bytes_DLEazSSSassQtQtSQISe(bytes, remaining, PROPERTIES,
                     &id,
                     pn_string_size(msg->user_id), pn_string_get(msg->user_id),
                     pn_string_bytes(msg->address),
                     pn_string_bytes(msg->subject),
                     pn_string_bytes(msg->reply_to),
                     &correlation_id,
                     pn_string_bytes(msg->content_type),
                     pn_string_bytes(msg->content_encoding),
                     (bool)msg->expiry_time, msg->expiry_time,
                     (bool)msg->creation_time, msg->creation_time,
                     pn_string_bytes(msg->group_id),
                     /*
                      * As a heuristic, null out group_sequence if there is no group_id and
                      * group_sequence is 0. In this case it is extremely unlikely we want
                      * group semantics
                      */
                     (bool)pn_string_get(msg->group_id) || (bool)msg->group_sequence , msg->group_sequence,
                     pn_string_bytes(msg->reply_to_group_id));
  if (last_size > remaining) return PN_OVERFLOW;

  remaining -= last_size;
  bytes += last_size;
  total += last_size;

  if (msg->properties_raw.size>0) {
    last_size = pn_amqp_encode_bytes_DLR(bytes, remaining, APPLICATION_PROPERTIES, msg->properties_raw);
    if (last_size > remaining) return PN_OVERFLOW;

    remaining -= last_size;
    bytes += last_size;
    total += last_size;
  }

  if (msg->body_raw.size>0) {
    uint64_t descriptor = AMQP_VALUE;
    if (msg->inferred) {
      switch ((uint8_t)msg->body_raw.start[0]) {
      case PNE_VBIN8:
      case PNE_VBIN32:
        descriptor = DATA;
        break;
      case PNE_LIST0:
      case PNE_LIST8:
      case PNE_LIST32:
        descriptor = AMQP_SEQUENCE;
        break;
      }
    }
    last_size = pn_amqp_encode_bytes_DLR(bytes, remaining, descriptor, msg->body_raw);
    if (last_size > remaining) return PN_OVERFLOW;

    remaining -= last_size;
    bytes += last_size;
    total += last_size;
  }

  *isize = total;
  return 0;
}

int pn_message_data(pn_message_t *msg, pn_data_t *data)
{
  pn_data_clear(data);
  int err = pn_data_fill(data, "DL[?o?B?I?o?I]", HEADER,
                         msg->durable, msg->durable,
                         msg->priority!=HEADER_PRIORITY_DEFAULT, msg->priority,
                         (bool)msg->ttl, msg->ttl,
                         msg->first_acquirer, msg->first_acquirer,
                         (bool)msg->delivery_count, msg->delivery_count);
  if (err)
    return pn_error_format(msg->error, err, "data error: %s",
                           pn_error_text(pn_data_error(data)));

  if (pn_data_size(msg->instructions_deprecated)) {
    pn_data_rewind(msg->instructions_deprecated);
    err = pn_data_fill(data, "DLC", DELIVERY_ANNOTATIONS, msg->instructions_deprecated);
    if (err)
      return pn_error_format(msg->error, err, "data error: %s",
                             pn_error_text(pn_data_error(data)));
  }

  if (pn_data_size(msg->annotations_deprecated)) {
    pn_data_rewind(msg->annotations_deprecated);
    err = pn_data_fill(data, "DLC", MESSAGE_ANNOTATIONS, msg->annotations_deprecated);
    if (err)
      return pn_error_format(msg->error, err, "data error: %s",
                             pn_error_text(pn_data_error(data)));
  }

  pn_atom_t id = pn_message_get_id(msg);
  pn_atom_t correlation_id = pn_message_get_correlation_id(msg);
  err = pn_data_fill(data, "DL[azSSSass?t?tS?IS]", PROPERTIES,
                     &id,
                     pn_string_size(msg->user_id), pn_string_get(msg->user_id),
                     pn_string_get(msg->address),
                     pn_string_get(msg->subject),
                     pn_string_get(msg->reply_to),
                     &correlation_id,
                     pn_string_get(msg->content_type),
                     pn_string_get(msg->content_encoding),
                     (bool)msg->expiry_time, msg->expiry_time,
                     (bool)msg->creation_time, msg->creation_time,
                     pn_string_get(msg->group_id),
                     /*
                      * As a heuristic, null out group_sequence if there is no group_id and
                      * group_sequence is 0. In this case it is extremely unlikely we want
                      * group semantics
                      */
                     (bool)pn_string_get(msg->group_id) || (bool)msg->group_sequence , msg->group_sequence,
                     pn_string_get(msg->reply_to_group_id));
  if (err)
    return pn_error_format(msg->error, err, "data error: %s",
                           pn_error_text(pn_data_error(data)));

  if (pn_data_size(msg->properties_deprecated)) {
    pn_data_rewind(msg->properties_deprecated);
    err = pn_data_fill(data, "DLC", APPLICATION_PROPERTIES, msg->properties_deprecated);
    if (err)
      return pn_error_format(msg->error, err, "data error: %s",
                             pn_error_text(pn_data_error(data)));
  }

  if (pn_data_size(msg->body_deprecated)) {
    pn_data_rewind(msg->body_deprecated);
    pn_data_next(msg->body_deprecated);
    pn_type_t body_type = pn_data_type(msg->body_deprecated);

    uint64_t descriptor = AMQP_VALUE;
    if (msg->inferred) {
      switch (body_type) {
        case PN_BINARY:
          descriptor = DATA;
          break;
        case PN_LIST:
          descriptor = AMQP_SEQUENCE;
          break;
        default:
          break;
      }
    }

    pn_data_rewind(msg->body_deprecated);
    err = pn_data_fill(data, "DLC", descriptor, msg->body_deprecated);
    if (err)
      return pn_error_format(msg->error, err, "data error: %s",
                             pn_error_text(pn_data_error(data)));
  }
  return 0;
}

pn_data_t *pn_message_instructions(pn_message_t *msg)
{
  if (!msg) return NULL;
  pni_switch_to_data(&msg->instructions_raw, &msg->instructions_deprecated);
  return msg->instructions_deprecated;
}

pn_data_t *pn_message_annotations(pn_message_t *msg)
{
  if (!msg) return NULL;
  pni_switch_to_data(&msg->annotations_raw, &msg->annotations_deprecated);
  return msg->annotations_deprecated;
}

pn_data_t *pn_message_properties(pn_message_t *msg)
{
  if (!msg) return NULL;
  pni_switch_to_data(&msg->properties_raw, &msg->properties_deprecated);
  return msg->properties_deprecated;
}

pn_data_t *pn_message_body(pn_message_t *msg)
{
  if (!msg) return NULL;
  pni_switch_to_data(&msg->body_raw, &msg->body_deprecated);
  return msg->body_deprecated;
}

ssize_t pn_message_encode2(pn_message_t *msg, pn_rwbytes_t *buffer) {
  static const size_t initial_size = 256;
  int err = 0;
  size_t size = 0;

  if (buffer->start == NULL) {
    buffer->start = (char*)malloc(initial_size);
    buffer->size = initial_size;
  }
  if (buffer->start == NULL) return PN_OUT_OF_MEMORY;
  size = buffer->size;
  while ((err = pn_message_encode(msg, buffer->start, &size)) == PN_OVERFLOW) {
    buffer->size *= 2;
    buffer->start = (char*)realloc(buffer->start, buffer->size);
    if (buffer->start == NULL) return PN_OUT_OF_MEMORY;
    size = buffer->size;
  }
  return err == 0 ? (ssize_t)size : err;
}

ssize_t pn_message_send(pn_message_t *msg, pn_link_t *sender, pn_rwbytes_t *buffer) {
  pn_rwbytes_t local_buf = { 0 };
  if (!buffer) buffer = &local_buf;
  ssize_t ret = pn_message_encode2(msg, buffer);
  if (ret >= 0) {
    ret = pn_link_send(sender, buffer->start, ret);
    if (ret >= 0) ret = pn_link_advance(sender);
    if (ret < 0) pn_error_copy(pn_message_error(msg), pn_link_error(sender));
  }
  if (local_buf.start) free(local_buf.start);
  return ret;
}
