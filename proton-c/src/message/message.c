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
#include <proton/buffer.h>
#include <proton/codec.h>
#include <proton/error.h>
#include <proton/parser.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "protocol.h"
#include "../util.h"

ssize_t pn_message_data(char *dst, size_t available, const char *src, size_t size)
{
  pn_data_t *data = pn_data(16);
  pn_data_put_described(data);
  pn_data_enter(data);
  pn_data_put_long(data, 0x75);
  pn_data_put_binary(data, pn_bytes(size, (char *)src));
  pn_data_exit(data);
  pn_data_rewind(data);
  int err = pn_data_encode(data, dst, available);
  pn_data_free(data);
  return err;
}

// message

struct pn_message_t {
  bool durable;
  uint8_t priority;
  pn_millis_t ttl;
  bool first_acquirer;
  uint32_t delivery_count;
  pn_data_t *id;
  pn_buffer_t *user_id;
  pn_buffer_t *address;
  pn_buffer_t *subject;
  pn_buffer_t *reply_to;
  pn_data_t *correlation_id;
  pn_buffer_t *content_type;
  pn_buffer_t *content_encoding;
  pn_timestamp_t expiry_time;
  pn_timestamp_t creation_time;
  pn_buffer_t *group_id;
  pn_sequence_t group_sequence;
  pn_buffer_t *reply_to_group_id;

  bool inferred;
  pn_data_t *data;
  pn_data_t *instructions;
  pn_data_t *annotations;
  pn_data_t *properties;
  pn_data_t *body;

  pn_format_t format;
  pn_parser_t *parser;
  pn_error_t *error;
};

pn_message_t *pn_message()
{
  pn_message_t *msg = malloc(sizeof(pn_message_t));
  msg->durable = false;
  msg->priority = PN_DEFAULT_PRIORITY;
  msg->ttl = 0;
  msg->first_acquirer = false;
  msg->delivery_count = 0;
  msg->id = pn_data(1);
  msg->user_id = NULL;
  msg->address = NULL;
  msg->subject = NULL;
  msg->reply_to = NULL;
  msg->correlation_id = pn_data(1);
  msg->content_type = NULL;
  msg->content_encoding = NULL;
  msg->expiry_time = 0;
  msg->creation_time = 0;
  msg->group_id = NULL;
  msg->group_sequence = 0;
  msg->reply_to_group_id = NULL;

  msg->inferred = false;
  msg->data = pn_data(16);
  msg->instructions = pn_data(16);
  msg->annotations = pn_data(16);
  msg->properties = pn_data(16);
  msg->body = pn_data(16);

  msg->format = PN_DATA;
  msg->parser = NULL;
  msg->error = pn_error();
  return msg;
}

void pn_message_free(pn_message_t *msg)
{
  if (msg) {
    pn_buffer_free(msg->user_id);
    pn_buffer_free(msg->address);
    pn_buffer_free(msg->subject);
    pn_buffer_free(msg->reply_to);
    pn_buffer_free(msg->content_type);
    pn_buffer_free(msg->content_encoding);
    pn_buffer_free(msg->group_id);
    pn_buffer_free(msg->reply_to_group_id);
    pn_data_free(msg->id);
    pn_data_free(msg->correlation_id);
    pn_data_free(msg->data);
    pn_data_free(msg->instructions);
    pn_data_free(msg->annotations);
    pn_data_free(msg->properties);
    pn_data_free(msg->body);
    pn_parser_free(msg->parser);
    pn_error_free(msg->error);
    free(msg);
  }
}

void pn_message_clear(pn_message_t *msg)
{
  msg->durable = false;
  msg->priority = PN_DEFAULT_PRIORITY;
  msg->ttl = 0;
  msg->first_acquirer = false;
  msg->delivery_count = 0;
  pn_data_clear(msg->id);
  if (msg->user_id) pn_buffer_clear(msg->user_id);
  if (msg->address) pn_buffer_clear(msg->address);
  if (msg->subject) pn_buffer_clear(msg->subject);
  if (msg->reply_to) pn_buffer_clear(msg->reply_to);
  pn_data_clear(msg->correlation_id);
  if (msg->content_type) pn_buffer_clear(msg->content_type);
  if (msg->content_encoding) pn_buffer_clear(msg->content_encoding);
  msg->expiry_time = 0;
  msg->creation_time = 0;
  if (msg->group_id) pn_buffer_clear(msg->group_id);
  msg->group_sequence = 0;
  if (msg->reply_to_group_id) pn_buffer_clear(msg->reply_to_group_id);
  msg->inferred = false;
  pn_data_clear(msg->data);
  pn_data_clear(msg->instructions);
  pn_data_clear(msg->annotations);
  pn_data_clear(msg->properties);
  pn_data_clear(msg->body);
}

int pn_message_errno(pn_message_t *msg)
{
  if (msg) {
    return pn_error_code(msg->error);
  } else {
    return 0;
  }
}

const char *pn_message_error(pn_message_t *msg)
{
  if (msg) {
    return pn_error_text(msg->error);
  } else {
    return NULL;
  }
}

bool pn_message_is_inferred(pn_message_t *msg)
{
  return msg ? msg->inferred : false;
}

int pn_message_set_inferred(pn_message_t *msg, bool inferred)
{
  if (!msg) return PN_ARG_ERR;
  msg->inferred = inferred;
  return 0;
}

pn_parser_t *pn_message_parser(pn_message_t *msg)
{
  if (!msg->parser) {
    msg->parser = pn_parser();
  }
  return msg->parser;
}

bool pn_message_is_durable(pn_message_t *msg)
{
  return msg ? msg->durable : false;
}
int pn_message_set_durable(pn_message_t *msg, bool durable)
{
  if (!msg) return PN_ARG_ERR;
  msg->durable = durable;
  return 0;
}


uint8_t pn_message_get_priority(pn_message_t *msg)
{
  return msg ? msg->priority : PN_DEFAULT_PRIORITY;
}
int pn_message_set_priority(pn_message_t *msg, uint8_t priority)
{
  if (!msg) return PN_ARG_ERR;
  msg->priority = priority;
  return 0;
}

pn_millis_t pn_message_get_ttl(pn_message_t *msg)
{
  return msg ? msg->ttl : 0;
}
int pn_message_set_ttl(pn_message_t *msg, pn_millis_t ttl)
{
  if (!msg) return PN_ARG_ERR;
  msg->ttl = ttl;
  return 0;
}

bool pn_message_is_first_acquirer(pn_message_t *msg)
{
  return msg ? msg->first_acquirer : false;
}
int pn_message_set_first_acquirer(pn_message_t *msg, bool first)
{
  if (!msg) return PN_ARG_ERR;
  msg->first_acquirer = first;
  return 0;
}

uint32_t pn_message_get_delivery_count(pn_message_t *msg)
{
  return msg ? msg->delivery_count : 0;
}
int pn_message_set_delivery_count(pn_message_t *msg, uint32_t count)
{
  if (!msg) return PN_ARG_ERR;
  msg->delivery_count = count;
  return 0;
}

pn_data_t *pn_message_id(pn_message_t *msg)
{
  return msg ? msg->id : NULL;
}
pn_atom_t pn_message_get_id(pn_message_t *msg)
{
  return msg ? pn_data_get_atom(msg->id) : (pn_atom_t) {.type=PN_NULL};
}
int pn_message_set_id(pn_message_t *msg, pn_atom_t id)
{
  if (!msg) return PN_ARG_ERR;

  pn_data_rewind(msg->id);
  return pn_data_put_atom(msg->id, id);
}

static int pn_buffer_set_bytes(pn_buffer_t **buf, pn_bytes_t bytes)
{
  if (!*buf) {
    *buf = pn_buffer(64);
  }

  pn_buffer_clear(*buf);

  return pn_buffer_append(*buf, bytes.start, bytes.size);
}

static const char *pn_buffer_str(pn_buffer_t *buf)
{
  if (buf) {
    pn_bytes_t bytes = pn_buffer_bytes(buf);
    if (bytes.size) {
      return bytes.start;
    }
  }

  return NULL;
}

static int pn_buffer_set_strn(pn_buffer_t **buf, const char *str, size_t size)
{
  if (!*buf) {
    *buf = pn_buffer(64);
  }

  pn_buffer_clear(*buf);
  int err = pn_buffer_append(*buf, str, size);
  if (err) return err;
  if (str && str[size-1]) {
    return pn_buffer_append(*buf, "\0", 1);
  } else {
    return 0;
  }
}

static int pn_buffer_set_str(pn_buffer_t **buf, const char *str)
{
  size_t size = str ? strlen(str) + 1 : 0;
  return pn_buffer_set_strn(buf, str, size);
}

pn_bytes_t pn_message_get_user_id(pn_message_t *msg)
{
  return msg && msg->user_id ? pn_buffer_bytes(msg->user_id) : pn_bytes(0, NULL);
}
int pn_message_set_user_id(pn_message_t *msg, pn_bytes_t user_id)
{
  if (!msg) return PN_ARG_ERR;
  return pn_buffer_set_bytes(&msg->user_id, user_id);
}

const char *pn_message_get_address(pn_message_t *msg)
{
  return msg ? pn_buffer_str(msg->address) : NULL;
}
int pn_message_set_address(pn_message_t *msg, const char *address)
{
  if (!msg) return PN_ARG_ERR;
  return pn_buffer_set_str(&msg->address, address);
}

const char *pn_message_get_subject(pn_message_t *msg)
{
  return msg ? pn_buffer_str(msg->subject) : NULL;
}
int pn_message_set_subject(pn_message_t *msg, const char *subject)
{
  if (!msg) return PN_ARG_ERR;
  return pn_buffer_set_str(&msg->subject, subject);
}

const char *pn_message_get_reply_to(pn_message_t *msg)
{
  return msg ? pn_buffer_str(msg->reply_to) : NULL;
}
int pn_message_set_reply_to(pn_message_t *msg, const char *reply_to)
{
  if (!msg) return PN_ARG_ERR;
  return pn_buffer_set_str(&msg->reply_to, reply_to);
}

pn_data_t *pn_message_correlation_id(pn_message_t *msg)
{
  return msg ? msg->correlation_id : NULL;
}
pn_atom_t pn_message_get_correlation_id(pn_message_t *msg)
{
  return msg ? pn_data_get_atom(msg->correlation_id) : (pn_atom_t) {.type=PN_NULL};
}
int pn_message_set_correlation_id(pn_message_t *msg, pn_atom_t atom)
{
  if (!msg) return PN_ARG_ERR;

  pn_data_rewind(msg->correlation_id);
  return pn_data_put_atom(msg->correlation_id, atom);
}

const char *pn_message_get_content_type(pn_message_t *msg)
{
  return msg ? pn_buffer_str(msg->content_type) : NULL;
}
int pn_message_set_content_type(pn_message_t *msg, const char *type)
{
  if (!msg) return PN_ARG_ERR;
  return pn_buffer_set_str(&msg->content_type, type);
}

const char *pn_message_get_content_encoding(pn_message_t *msg)
{
  return msg ? pn_buffer_str(msg->content_encoding) : NULL;
}
int pn_message_set_content_encoding(pn_message_t *msg, const char *encoding)
{
  if (!msg) return PN_ARG_ERR;
  return pn_buffer_set_str(&msg->content_encoding, encoding);
}

pn_timestamp_t pn_message_get_expiry_time(pn_message_t *msg)
{
  return msg ? msg->expiry_time : 0;
}
int pn_message_set_expiry_time(pn_message_t *msg, pn_timestamp_t time)
{
  if (!msg) return PN_ARG_ERR;
  msg->expiry_time = time;
  return 0;
}

pn_timestamp_t pn_message_get_creation_time(pn_message_t *msg)
{
  return msg ? msg->creation_time : 0;
}
int pn_message_set_creation_time(pn_message_t *msg, pn_timestamp_t time)
{
  if (!msg) return PN_ARG_ERR;
  msg->creation_time = time;
  return 0;
}

const char *pn_message_get_group_id(pn_message_t *msg)
{
  return msg ? pn_buffer_str(msg->group_id) : NULL;
}
int pn_message_set_group_id(pn_message_t *msg, const char *group_id)
{
  if (!msg) return PN_ARG_ERR;
  return pn_buffer_set_str(&msg->group_id, group_id);
}

pn_sequence_t pn_message_get_group_sequence(pn_message_t *msg)
{
  return msg ? msg->group_sequence : 0;
}
int pn_message_set_group_sequence(pn_message_t *msg, pn_sequence_t n)
{
  if (!msg) return PN_ARG_ERR;
  msg->group_sequence = n;
  return 0;
}

const char *pn_message_get_reply_to_group_id(pn_message_t *msg)
{
  return msg ? pn_buffer_str(msg->reply_to_group_id) : NULL;
}
int pn_message_set_reply_to_group_id(pn_message_t *msg, const char *reply_to_group_id)
{
  if (!msg) return PN_ARG_ERR;
  return pn_buffer_set_str(&msg->reply_to_group_id, reply_to_group_id);
}

int pn_message_decode(pn_message_t *msg, const char *bytes, size_t size)
{
  if (!msg || !bytes || !size) return PN_ARG_ERR;

  pn_message_clear(msg);

  while (size) {
    pn_data_clear(msg->data);
    ssize_t used = pn_data_decode(msg->data, bytes, size);
    if (used < 0) return pn_error_format(msg->error, used, "data error: %s",
                                         pn_data_error(msg->data));
    size -= used;
    bytes += used;
    bool scanned;
    uint64_t desc;
    int err = pn_data_scan(msg->data, "D?L.", &scanned, &desc);
    if (err) return pn_error_format(msg->error, err, "data error: %s",
                                    pn_data_error(msg->data));
    if (!scanned) {
      desc = 0;
    }

    pn_data_rewind(msg->data);
    pn_data_next(msg->data);
    pn_data_enter(msg->data);
    pn_data_next(msg->data);

    switch (desc) {
    case HEADER:
      pn_data_scan(msg->data, "D.[oBIoI]", &msg->durable, &msg->priority,
                   &msg->ttl, &msg->first_acquirer, &msg->delivery_count);
      break;
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
                                        pn_data_error(msg->data));
        err = pn_buffer_set_bytes(&msg->user_id, user_id);
        if (err) return pn_error_format(msg->error, err, "error setting user_id");
        err = pn_buffer_set_strn(&msg->address, address.start, address.size);
        if (err) return pn_error_format(msg->error, err, "error setting address");
        err = pn_buffer_set_strn(&msg->subject, subject.start, subject.size);
        if (err) return pn_error_format(msg->error, err, "error setting subject");
        err = pn_buffer_set_strn(&msg->reply_to, reply_to.start, reply_to.size);
        if (err) return pn_error_format(msg->error, err, "error setting reply_to");
        err = pn_buffer_set_strn(&msg->content_type, ctype.start, ctype.size);
        if (err) return pn_error_format(msg->error, err, "error setting content_type");
        err = pn_buffer_set_strn(&msg->content_encoding, cencoding.start,
                                 cencoding.size);
        if (err) return pn_error_format(msg->error, err, "error setting content_encoding");
        err = pn_buffer_set_strn(&msg->group_id, group_id.start, group_id.size);
        if (err) return pn_error_format(msg->error, err, "error setting group_id");
        err = pn_buffer_set_strn(&msg->reply_to_group_id, reply_to_group_id.start,
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
    case AMQP_VALUE:
      pn_data_narrow(msg->data);
      err = pn_data_copy(msg->body, msg->data);
      if (err) return err;
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

  int err = pn_data_fill(msg->data, "DL[oB?IoI]", HEADER, msg->durable,
                         msg->priority, msg->ttl, msg->ttl, msg->first_acquirer,
                         msg->delivery_count);
  if (err)
    return pn_error_format(msg->error, err, "data error: %s",
                           pn_data_error(msg->data));

  if (pn_data_size(msg->instructions)) {
    pn_data_put_described(msg->data);
    pn_data_enter(msg->data);
    pn_data_put_ulong(msg->data, DELIVERY_ANNOTATIONS);
    pn_data_rewind(msg->instructions);
    err = pn_data_append(msg->data, msg->instructions);
    if (err)
      return pn_error_format(msg->error, err, "data error: %s",
                             pn_data_error(msg->data));
    pn_data_exit(msg->data);
  }

  if (pn_data_size(msg->annotations)) {
    pn_data_put_described(msg->data);
    pn_data_enter(msg->data);
    pn_data_put_ulong(msg->data, MESSAGE_ANNOTATIONS);
    pn_data_rewind(msg->annotations);
    err = pn_data_append(msg->data, msg->annotations);
    if (err)
      return pn_error_format(msg->error, err, "data error: %s",
                             pn_data_error(msg->data));
    pn_data_exit(msg->data);
  }

  err = pn_data_fill(msg->data, "DL[CzSSSCssttSIS]", PROPERTIES,
                     msg->id,
                     pn_buffer_bytes(msg->user_id),
                     pn_buffer_str(msg->address),
                     pn_buffer_str(msg->subject),
                     pn_buffer_str(msg->reply_to),
                     msg->correlation_id,
                     pn_buffer_str(msg->content_type),
                     pn_buffer_str(msg->content_encoding),
                     msg->expiry_time,
                     msg->creation_time,
                     pn_buffer_str(msg->group_id),
                     msg->group_sequence,
                     pn_buffer_str(msg->reply_to_group_id));
  if (err)
    return pn_error_format(msg->error, err, "data error: %s",
                           pn_data_error(msg->data));

  if (pn_data_size(msg->properties)) {
    pn_data_put_described(msg->data);
    pn_data_enter(msg->data);
    pn_data_put_ulong(msg->data, APPLICATION_PROPERTIES);
    pn_data_rewind(msg->properties);
    err = pn_data_append(msg->data, msg->properties);
    if (err)
      return pn_error_format(msg->error, err, "data error: %s",
                             pn_data_error(msg->data));
    pn_data_exit(msg->data);
  }

  if (pn_data_size(msg->body)) {
    pn_data_rewind(msg->body);
    pn_data_next(msg->body);
    pn_type_t body_type = pn_data_type(msg->body);
    pn_data_rewind(msg->body);

    pn_data_put_described(msg->data);
    pn_data_enter(msg->data);
    if (msg->inferred) {
      switch (body_type) {
      case PN_BINARY:
        pn_data_put_ulong(msg->data, DATA);
        break;
      case PN_LIST:
        pn_data_put_ulong(msg->data, AMQP_SEQUENCE);
        break;
      default:
        pn_data_put_ulong(msg->data, AMQP_VALUE);
        break;
      }
    } else {
      pn_data_put_ulong(msg->data, AMQP_VALUE);
    }
    pn_data_append(msg->data, msg->body);
  }

  size_t remaining = *size;
  ssize_t encoded = pn_data_encode(msg->data, bytes, remaining);
  if (encoded < 0)
    return pn_error_format(msg->error, encoded, "data error: %s",
                           pn_data_error(msg->data));

  bytes += encoded;
  remaining -= encoded;

  *size -= remaining;

  pn_data_clear(msg->data);

  return 0;
}

pn_format_t pn_message_get_format(pn_message_t *msg)
{
  return msg ? msg->format : PN_AMQP;
}

int pn_message_set_format(pn_message_t *msg, pn_format_t format)
{
  if (!msg) return PN_ARG_ERR;

  msg->format = format;
  return 0;
}

int pn_message_load(pn_message_t *msg, const char *data, size_t size)
{
  if (!msg) return PN_ARG_ERR;

  switch (msg->format) {
  case PN_DATA: return pn_message_load_data(msg, data, size);
  case PN_TEXT: return pn_message_load_text(msg, data, size);
  case PN_AMQP: return pn_message_load_amqp(msg, data, size);
  case PN_JSON: return pn_message_load_json(msg, data, size);
  }

  return PN_STATE_ERR;
}

int pn_message_load_data(pn_message_t *msg, const char *data, size_t size)
{
  if (!msg) return PN_ARG_ERR;

  pn_data_clear(msg->body);
  int err = pn_data_fill(msg->body, "z", size, data);
  if (err) {
    return pn_error_format(msg->error, err, "data error: %s",
                           pn_data_error(msg->body));
  } else {
    return 0;
  }
}

int pn_message_load_text(pn_message_t *msg, const char *data, size_t size)
{
  if (!msg) return PN_ARG_ERR;

  pn_data_clear(msg->body);
  int err = pn_data_fill(msg->body, "S", data);
  if (err) {
    return pn_error_format(msg->error, err, "data error: %s",
                           pn_data_error(msg->body));
  } else {
    return 0;
  }
}

int pn_message_load_amqp(pn_message_t *msg, const char *data, size_t size)
{
  if (!msg) return PN_ARG_ERR;

  pn_parser_t *parser = pn_message_parser(msg);

  pn_data_clear(msg->body);
  int err = pn_parser_parse(parser, data, msg->body);
  if (err) {
    return pn_error_format(msg->error, err, "parse error: %s",
                           pn_parser_error(parser));
  } else {
    return 0;
  }
}

int pn_message_load_json(pn_message_t *msg, const char *data, size_t size)
{
  if (!msg) return PN_ARG_ERR;

  // XXX: unsupported format

  return PN_ERR;
}

int pn_message_save(pn_message_t *msg, char *data, size_t *size)
{
  if (!msg) return PN_ARG_ERR;

  switch (msg->format) {
  case PN_DATA: return pn_message_save_data(msg, data, size);
  case PN_TEXT: return pn_message_save_text(msg, data, size);
  case PN_AMQP: return pn_message_save_amqp(msg, data, size);
  case PN_JSON: return pn_message_save_json(msg, data, size);
  }

  return PN_STATE_ERR;
}

int pn_message_save_data(pn_message_t *msg, char *data, size_t *size)
{
  if (!msg) return PN_ARG_ERR;

  if (!msg->body || pn_data_size(msg->body) == 0) {
    *size = 0;
    return 0;
  }

  bool scanned;
  pn_bytes_t bytes;
  int err = pn_data_scan(msg->body, "?z", &scanned, &bytes);
  if (err) return pn_error_format(msg->error, err, "data error: %s",
                                  pn_data_error(msg->body));
  if (scanned) {
    if (bytes.size > *size) {
      return PN_OVERFLOW;
    } else {
      memcpy(data, bytes.start, bytes.size);
      *size = bytes.size;
      return 0;
    }
  } else {
    return PN_STATE_ERR;
  }
}

int pn_message_save_text(pn_message_t *msg, char *data, size_t *size)
{
  if (!msg) return PN_ARG_ERR;

  pn_data_rewind(msg->body);
  if (pn_data_next(msg->body)) {
    switch (pn_data_type(msg->body)) {
    case PN_STRING:
      {
        pn_bytes_t str = pn_data_get_bytes(msg->body);
        if (str.size >= *size) {
          return PN_OVERFLOW;
        } else {
          memcpy(data, str.start, str.size);
          data[str.size] = '\0';
          *size = str.size;
          return 0;
        }
      }
      break;
    case PN_NULL:
      *size = 0;
      return 0;
    default:
      return PN_STATE_ERR;
    }
  } else {
    *size = 0;
    return 0;
  }
}

int pn_message_save_amqp(pn_message_t *msg, char *data, size_t *size)
{
  if (!msg) return PN_ARG_ERR;

  if (!msg->body) {
    *size = 0;
    return 0;
  }

  int err = pn_data_format(msg->body, data, size);
  if (err) return pn_error_format(msg->error, err, "data error: %s",
                                  pn_data_error(msg->body));

  return 0;
}

int pn_message_save_json(pn_message_t *msg, char *data, size_t *size)
{
  if (!msg) return PN_ARG_ERR;

  // XXX: unsupported format

  return PN_ERR;
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
