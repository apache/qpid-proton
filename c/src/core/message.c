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

#include "max_align.h"
#include "message-internal.h"
#include "protocol.h"
#include "util.h"

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
  pn_timestamp_t expiry_time;
  pn_timestamp_t creation_time;
  pn_data_t *id;
  pn_string_t *user_id;
  pn_string_t *address;
  pn_string_t *subject;
  pn_string_t *reply_to;
  pn_data_t *correlation_id;
  pn_string_t *content_type;
  pn_string_t *content_encoding;
  pn_string_t *group_id;
  pn_string_t *reply_to_group_id;

  pn_data_t *data;
  pn_data_t *instructions;
  pn_data_t *annotations;
  pn_data_t *properties;
  pn_data_t *body;

  pn_error_t *error;

  pn_sequence_t group_sequence;
  pn_millis_t ttl;
  uint32_t delivery_count;

  uint8_t priority;

  bool durable;
  bool first_acquirer;
  bool inferred;
};

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
  pn_data_free(msg->id);
  pn_data_free(msg->correlation_id);
  pn_data_free(msg->data);
  pn_data_free(msg->instructions);
  pn_data_free(msg->annotations);
  pn_data_free(msg->properties);
  pn_data_free(msg->body);
  pn_error_free(msg->error);
}

int pn_message_inspect(void *obj, pn_string_t *dst)
{
  pn_message_t *msg = (pn_message_t *) obj;
  int err = pn_string_addf(dst, "Message{");
  if (err) return err;

  bool comma = false;

  if (pn_string_get(msg->address)) {
    err = pn_string_addf(dst, "address=");
    if (err) return err;
    err = pn_inspect(msg->address, dst);
    if (err) return err;
    err = pn_string_addf(dst, ", ");
    if (err) return err;
    comma = true;
  }

  if (msg->durable) {
    err = pn_string_addf(dst, "durable=%i, ", msg->durable);
    if (err) return err;
    comma = true;
  }

  if (msg->priority != HEADER_PRIORITY_DEFAULT) {
    err = pn_string_addf(dst, "priority=%i, ", msg->priority);
    if (err) return err;
    comma = true;
  }

  if (msg->ttl) {
    err = pn_string_addf(dst, "ttl=%" PRIu32 ", ", msg->ttl);
    if (err) return err;
    comma = true;
  }

  if (msg->first_acquirer) {
    err = pn_string_addf(dst, "first_acquirer=%i, ", msg->first_acquirer);
    if (err) return err;
    comma = true;
  }

  if (msg->delivery_count) {
    err = pn_string_addf(dst, "delivery_count=%" PRIu32 ", ", msg->delivery_count);
    if (err) return err;
    comma = true;
  }

  if (pn_data_size(msg->id)) {
    err = pn_string_addf(dst, "id=");
    if (err) return err;
    err = pn_inspect(msg->id, dst);
    if (err) return err;
    err = pn_string_addf(dst, ", ");
    if (err) return err;
    comma = true;
  }

  if (pn_string_get(msg->user_id)) {
    err = pn_string_addf(dst, "user_id=");
    if (err) return err;
    err = pn_inspect(msg->user_id, dst);
    if (err) return err;
    err = pn_string_addf(dst, ", ");
    if (err) return err;
    comma = true;
  }

  if (pn_string_get(msg->subject)) {
    err = pn_string_addf(dst, "subject=");
    if (err) return err;
    err = pn_inspect(msg->subject, dst);
    if (err) return err;
    err = pn_string_addf(dst, ", ");
    if (err) return err;
    comma = true;
  }

  if (pn_string_get(msg->reply_to)) {
    err = pn_string_addf(dst, "reply_to=");
    if (err) return err;
    err = pn_inspect(msg->reply_to, dst);
    if (err) return err;
    err = pn_string_addf(dst, ", ");
    if (err) return err;
    comma = true;
  }

  if (pn_data_size(msg->correlation_id)) {
    err = pn_string_addf(dst, "correlation_id=");
    if (err) return err;
    err = pn_inspect(msg->correlation_id, dst);
    if (err) return err;
    err = pn_string_addf(dst, ", ");
    if (err) return err;
    comma = true;
  }

  if (pn_string_get(msg->content_type)) {
    err = pn_string_addf(dst, "content_type=");
    if (err) return err;
    err = pn_inspect(msg->content_type, dst);
    if (err) return err;
    err = pn_string_addf(dst, ", ");
    if (err) return err;
    comma = true;
  }

  if (pn_string_get(msg->content_encoding)) {
    err = pn_string_addf(dst, "content_encoding=");
    if (err) return err;
    err = pn_inspect(msg->content_encoding, dst);
    if (err) return err;
    err = pn_string_addf(dst, ", ");
    if (err) return err;
    comma = true;
  }

  if (msg->expiry_time) {
    err = pn_string_addf(dst, "expiry_time=%" PRIi64 ", ", msg->expiry_time);
    if (err) return err;
    comma = true;
  }

  if (msg->creation_time) {
    err = pn_string_addf(dst, "creation_time=%" PRIi64 ", ", msg->creation_time);
    if (err) return err;
    comma = true;
  }

  if (pn_string_get(msg->group_id)) {
    err = pn_string_addf(dst, "group_id=");
    if (err) return err;
    err = pn_inspect(msg->group_id, dst);
    if (err) return err;
    err = pn_string_addf(dst, ", ");
    if (err) return err;
    comma = true;
  }

  if (msg->group_sequence) {
    err = pn_string_addf(dst, "group_sequence=%" PRIi32 ", ", msg->group_sequence);
    if (err) return err;
    comma = true;
  }

  if (pn_string_get(msg->reply_to_group_id)) {
    err = pn_string_addf(dst, "reply_to_group_id=");
    if (err) return err;
    err = pn_inspect(msg->reply_to_group_id, dst);
    if (err) return err;
    err = pn_string_addf(dst, ", ");
    if (err) return err;
    comma = true;
  }

  if (msg->inferred) {
    err = pn_string_addf(dst, "inferred=%i, ", msg->inferred);
    if (err) return err;
    comma = true;
  }

  if (pn_data_size(msg->instructions)) {
    err = pn_string_addf(dst, "instructions=");
    if (err) return err;
    err = pn_inspect(msg->instructions, dst);
    if (err) return err;
    err = pn_string_addf(dst, ", ");
    if (err) return err;
    comma = true;
  }

  if (pn_data_size(msg->annotations)) {
    err = pn_string_addf(dst, "annotations=");
    if (err) return err;
    err = pn_inspect(msg->annotations, dst);
    if (err) return err;
    err = pn_string_addf(dst, ", ");
    if (err) return err;
    comma = true;
  }

  if (pn_data_size(msg->properties)) {
    err = pn_string_addf(dst, "properties=");
    if (err) return err;
    err = pn_inspect(msg->properties, dst);
    if (err) return err;
    err = pn_string_addf(dst, ", ");
    if (err) return err;
    comma = true;
  }

  if (pn_data_size(msg->body)) {
    err = pn_string_addf(dst, "body=");
    if (err) return err;
    err = pn_inspect(msg->body, dst);
    if (err) return err;
    err = pn_string_addf(dst, ", ");
    if (err) return err;
    comma = true;
  }

  if (comma) {
    int err = pn_string_resize(dst, pn_string_size(dst) - 2);
    if (err) return err;
  }

  return pn_string_addf(dst, "}");
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
  msg->id = pn_data(1);
  msg->user_id = pn_string(NULL);
  msg->address = pn_string(NULL);
  msg->subject = pn_string(NULL);
  msg->reply_to = pn_string(NULL);
  msg->correlation_id = pn_data(1);
  msg->content_type = pn_string(NULL);
  msg->content_encoding = pn_string(NULL);
  msg->expiry_time = 0;
  msg->creation_time = 0;
  msg->group_id = pn_string(NULL);
  msg->group_sequence = 0;
  msg->reply_to_group_id = pn_string(NULL);

  msg->inferred = false;
  msg->data = pn_data(16);
  msg->instructions = pn_data(16);
  msg->annotations = pn_data(16);
  msg->properties = pn_data(16);
  msg->body = pn_data(16);

  msg->error = pn_error();
  return msg;
}

pn_message_t *pn_message() {
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
  pn_data_clear(msg->id);
  pn_string_clear(msg->user_id);
  pn_string_clear(msg->address);
  pn_string_clear(msg->subject);
  pn_string_clear(msg->reply_to);
  pn_data_clear(msg->correlation_id);
  pn_string_clear(msg->content_type);
  pn_string_clear(msg->content_encoding);
  msg->expiry_time = 0;
  msg->creation_time = 0;
  pn_string_clear(msg->group_id);
  msg->group_sequence = 0;
  pn_string_clear(msg->reply_to_group_id);
  msg->inferred = false;
  pn_data_clear(msg->data);
  pn_data_clear(msg->instructions);
  pn_data_clear(msg->annotations);
  pn_data_clear(msg->properties);
  pn_data_clear(msg->body);
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
  return msg->id;
}
pn_atom_t pn_message_get_id(pn_message_t *msg)
{
  assert(msg);
  return pn_data_get_atom(msg->id);
}
int pn_message_set_id(pn_message_t *msg, pn_atom_t id)
{
  assert(msg);
  pn_data_rewind(msg->id);
  return pn_data_put_atom(msg->id, id);
}

static pn_bytes_t pn_string_get_bytes(pn_string_t *string)
{
  return pn_bytes(pn_string_size(string), (char *) pn_string_get(string));
}

static int pn_string_set_bytes(pn_string_t *string, pn_bytes_t bytes)
{
  return pn_string_setn(string, bytes.start, bytes.size);
}

pn_bytes_t pn_message_get_user_id(pn_message_t *msg)
{
  assert(msg);
  return pn_string_get_bytes(msg->user_id);
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
  return msg->correlation_id;
}
pn_atom_t pn_message_get_correlation_id(pn_message_t *msg)
{
  assert(msg);
  return pn_data_get_atom(msg->correlation_id);
}
int pn_message_set_correlation_id(pn_message_t *msg, pn_atom_t atom)
{
  assert(msg);
  pn_data_rewind(msg->correlation_id);
  return pn_data_put_atom(msg->correlation_id, atom);
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

  pn_message_clear(msg);

  while (size) {
    pn_data_clear(msg->data);
    ssize_t used = pn_data_decode(msg->data, bytes, size);
    if (used < 0)
        return pn_error_format(msg->error, used, "data error: %s",
                               pn_error_text(pn_data_error(msg->data)));
    size -= used;
    bytes += used;
    bool scanned;
    uint64_t desc;
    int err = pn_data_scan(msg->data, "D?L.", &scanned, &desc);
    if (err) return pn_error_format(msg->error, err, "data error: %s",
                                    pn_error_text(pn_data_error(msg->data)));
    if (!scanned) {
      desc = 0;
    }

    pn_data_rewind(msg->data);
    pn_data_next(msg->data);
    pn_data_enter(msg->data);
    pn_data_next(msg->data);

    switch (desc) {
    case HEADER: {
      bool priority_q;
      uint8_t priority;
      err = pn_data_scan(msg->data, "D.[o?BIoI]",
                         &msg->durable,
                         &priority_q, &priority,
                         &msg->ttl,
                         &msg->first_acquirer,
                         &msg->delivery_count);
      if (err) return pn_error_format(msg->error, err, "data error: %s",
                                      pn_error_text(pn_data_error(msg->data)));
      msg->priority = priority_q ? priority : HEADER_PRIORITY_DEFAULT;
      break;
    }
    case PROPERTIES:
      {
        pn_bytes_t user_id, address, subject, reply_to, ctype, cencoding,
          group_id, reply_to_group_id;
        pn_data_clear(msg->id);
        pn_data_clear(msg->correlation_id);
        err = pn_data_scan(msg->data, "D.[CzSSSCssttSIS]", msg->id,
                           &user_id, &address, &subject, &reply_to,
                           msg->correlation_id, &ctype, &cencoding,
                           &msg->expiry_time, &msg->creation_time, &group_id,
                           &msg->group_sequence, &reply_to_group_id);
        if (err) return pn_error_format(msg->error, err, "data error: %s",
                                        pn_error_text(pn_data_error(msg->data)));
        err = pn_string_set_bytes(msg->user_id, user_id);
        if (err) return pn_error_format(msg->error, err, "error setting user_id");
        err = pn_string_setn(msg->address, address.start, address.size);
        if (err) return pn_error_format(msg->error, err, "error setting address");
        err = pn_string_setn(msg->subject, subject.start, subject.size);
        if (err) return pn_error_format(msg->error, err, "error setting subject");
        err = pn_string_setn(msg->reply_to, reply_to.start, reply_to.size);
        if (err) return pn_error_format(msg->error, err, "error setting reply_to");
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
      }
      break;
    case DELIVERY_ANNOTATIONS:
      pn_data_narrow(msg->data);
      err = pn_data_copy(msg->instructions, msg->data);
      if (err) return err;
      break;
    case MESSAGE_ANNOTATIONS:
      pn_data_narrow(msg->data);
      err = pn_data_copy(msg->annotations, msg->data);
      if (err) return err;
      break;
    case APPLICATION_PROPERTIES:
      pn_data_narrow(msg->data);
      err = pn_data_copy(msg->properties, msg->data);
      if (err) return err;
      break;
    case DATA:
    case AMQP_SEQUENCE:
      msg->inferred = true;
      pn_data_narrow(msg->data);
      err = pn_data_copy(msg->body, msg->data);
      if (err) return err;
      break;
    case AMQP_VALUE:
      msg->inferred = false;
      pn_data_narrow(msg->data);
      err = pn_data_copy(msg->body, msg->data);
      if (err) return err;
      break;
    case FOOTER:
      break;
    default:
      err = pn_data_copy(msg->body, msg->data);
      if (err) return err;
      break;
    }
  }

  pn_data_clear(msg->data);
  return 0;
}

int pn_message_encode(pn_message_t *msg, char *bytes, size_t *size)
{
  if (!msg || !bytes || !size || !*size) return PN_ARG_ERR;
  pn_data_clear(msg->data);
  pn_message_data(msg, msg->data);
  size_t remaining = *size;
  ssize_t encoded = pn_data_encode(msg->data, bytes, remaining);
  if (encoded < 0) {
    if (encoded == PN_OVERFLOW) {
      return encoded;
    } else {
      return pn_error_format(msg->error, encoded, "data error: %s",
                             pn_error_text(pn_data_error(msg->data)));
    }
  }
  bytes += encoded;
  remaining -= encoded;
  *size -= remaining;
  pn_data_clear(msg->data);
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

  if (pn_data_size(msg->instructions)) {
    pn_data_put_described(data);
    pn_data_enter(data);
    pn_data_put_ulong(data, DELIVERY_ANNOTATIONS);
    pn_data_rewind(msg->instructions);
    err = pn_data_append(data, msg->instructions);
    if (err)
      return pn_error_format(msg->error, err, "data error: %s",
                             pn_error_text(pn_data_error(data)));
    pn_data_exit(data);
  }

  if (pn_data_size(msg->annotations)) {
    pn_data_put_described(data);
    pn_data_enter(data);
    pn_data_put_ulong(data, MESSAGE_ANNOTATIONS);
    pn_data_rewind(msg->annotations);
    err = pn_data_append(data, msg->annotations);
    if (err)
      return pn_error_format(msg->error, err, "data error: %s",
                             pn_error_text(pn_data_error(data)));
    pn_data_exit(data);
  }

  err = pn_data_fill(data, "DL[CzSSSCss?t?tS?IS]", PROPERTIES,
                     msg->id,
                     pn_string_size(msg->user_id), pn_string_get(msg->user_id),
                     pn_string_get(msg->address),
                     pn_string_get(msg->subject),
                     pn_string_get(msg->reply_to),
                     msg->correlation_id,
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

  if (pn_data_size(msg->properties)) {
    pn_data_put_described(data);
    pn_data_enter(data);
    pn_data_put_ulong(data, APPLICATION_PROPERTIES);
    pn_data_rewind(msg->properties);
    err = pn_data_append(data, msg->properties);
    if (err)
      return pn_error_format(msg->error, err, "data error: %s",
                             pn_error_text(pn_data_error(data)));
    pn_data_exit(data);
  }

  if (pn_data_size(msg->body)) {
    pn_data_rewind(msg->body);
    pn_data_next(msg->body);
    pn_type_t body_type = pn_data_type(msg->body);
    pn_data_rewind(msg->body);

    pn_data_put_described(data);
    pn_data_enter(data);
    if (msg->inferred) {
      switch (body_type) {
      case PN_BINARY:
        pn_data_put_ulong(data, DATA);
        break;
      case PN_LIST:
        pn_data_put_ulong(data, AMQP_SEQUENCE);
        break;
      default:
        pn_data_put_ulong(data, AMQP_VALUE);
        break;
      }
    } else {
      pn_data_put_ulong(data, AMQP_VALUE);
    }
    pn_data_append(data, msg->body);
  }
  return 0;
}

pn_data_t *pn_message_instructions(pn_message_t *msg)
{
  return msg ? msg->instructions : NULL;
}

pn_data_t *pn_message_annotations(pn_message_t *msg)
{
  return msg ? msg->annotations : NULL;
}

pn_data_t *pn_message_properties(pn_message_t *msg)
{
  return msg ? msg->properties : NULL;
}

pn_data_t *pn_message_body(pn_message_t *msg)
{
  return msg ? msg->body : NULL;
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
