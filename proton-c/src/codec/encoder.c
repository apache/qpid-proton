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

#include <proton/error.h>
#include <proton/object.h>
#include <proton/codec.h>
#include "encodings.h"
#include "encoder.h"

#include <string.h>

#include "data.h"

struct pn_encoder_t {
  char *output;
  size_t size;
  char *position;
  pn_error_t *error;
};

static void pn_encoder_initialize(void *obj)
{
  pn_encoder_t *encoder = (pn_encoder_t *) obj;
  encoder->output = NULL;
  encoder->size = 0;
  encoder->position = NULL;
  encoder->error = pn_error();
}

static void pn_encoder_finalize(void *obj) {
  pn_encoder_t *encoder = (pn_encoder_t *) obj;
  pn_error_free(encoder->error);
}

#define pn_encoder_hashcode NULL
#define pn_encoder_compare NULL
#define pn_encoder_inspect NULL

pn_encoder_t *pn_encoder()
{
  static const pn_class_t clazz = PN_CLASS(pn_encoder);
  return (pn_encoder_t *) pn_new(sizeof(pn_encoder_t), &clazz);
}

static uint8_t pn_type2code(pn_encoder_t *encoder, pn_type_t type)
{
  switch (type)
  {
  case PN_NULL: return PNE_NULL;
  case PN_BOOL: return PNE_BOOLEAN;
  case PN_UBYTE: return PNE_UBYTE;
  case PN_BYTE: return PNE_BYTE;
  case PN_USHORT: return PNE_USHORT;
  case PN_SHORT: return PNE_SHORT;
  case PN_UINT: return PNE_UINT;
  case PN_INT: return PNE_INT;
  case PN_CHAR: return PNE_UTF32;
  case PN_FLOAT: return PNE_FLOAT;
  case PN_LONG: return PNE_LONG;
  case PN_TIMESTAMP: return PNE_MS64;
  case PN_DOUBLE: return PNE_DOUBLE;
  case PN_DECIMAL32: return PNE_DECIMAL32;
  case PN_DECIMAL64: return PNE_DECIMAL64;
  case PN_DECIMAL128: return PNE_DECIMAL128;
  case PN_UUID: return PNE_UUID;
  case PN_ULONG: return PNE_ULONG;
  case PN_BINARY: return PNE_VBIN32;
  case PN_STRING: return PNE_STR32_UTF8;
  case PN_SYMBOL: return PNE_SYM32;
  case PN_LIST: return PNE_LIST32;
  case PN_ARRAY: return PNE_ARRAY32;
  case PN_MAP: return PNE_MAP32;
  case PN_DESCRIBED: return PNE_DESCRIPTOR;
  default:
    return pn_error_format(encoder->error, PN_ERR, "not a value type: %u\n", type);
  }
}

static uint8_t pn_node2code(pn_encoder_t *encoder, pni_node_t *node)
{
  switch (node->atom.type) {
  case PN_ULONG:
    if (node->atom.u.as_ulong < 256) {
      return PNE_SMALLULONG;
    } else {
      return PNE_ULONG;
    }
  case PN_UINT:
    if (node->atom.u.as_uint < 256) {
      return PNE_SMALLUINT;
    } else {
      return PNE_UINT;
    }
  case PN_BOOL:
    if (node->atom.u.as_bool) {
      return PNE_TRUE;
    } else {
      return PNE_FALSE;
    }
  case PN_STRING:
    if (node->atom.u.as_bytes.size < 256) {
      return PNE_STR8_UTF8;
    } else {
      return PNE_STR32_UTF8;
    }
  case PN_SYMBOL:
    if (node->atom.u.as_bytes.size < 256) {
      return PNE_SYM8;
    } else {
      return PNE_SYM32;
    }
  case PN_BINARY:
    if (node->atom.u.as_bytes.size < 256) {
      return PNE_VBIN8;
    } else {
      return PNE_VBIN32;
    }
  default:
    return pn_type2code(encoder, node->atom.type);
  }
}

static size_t pn_encoder_remaining(pn_encoder_t *encoder)
{
  return encoder->output + encoder->size - encoder->position;
}

static inline int pn_encoder_writef8(pn_encoder_t *encoder, uint8_t value)
{
  if (pn_encoder_remaining(encoder)) {
    encoder->position[0] = value;
    encoder->position++;
    return 0;
  } else {
    return PN_OVERFLOW;
  }
}

static inline int pn_encoder_writef16(pn_encoder_t *encoder, uint16_t value)
{
  if (pn_encoder_remaining(encoder) < 2) {
    return PN_OVERFLOW;
  } else {
    encoder->position[0] = 0xFF & (value >> 8);
    encoder->position[1] = 0xFF & (value     );
    encoder->position += 2;
    return 0;
  }
}

static inline int pn_encoder_writef32(pn_encoder_t *encoder, uint32_t value)
{
  if (pn_encoder_remaining(encoder) < 4) {
    return PN_OVERFLOW;
  } else {
    encoder->position[0] = 0xFF & (value >> 24);
    encoder->position[1] = 0xFF & (value >> 16);
    encoder->position[2] = 0xFF & (value >>  8);
    encoder->position[3] = 0xFF & (value      );
    encoder->position += 4;
    return 0;
  }
}

static inline int pn_encoder_writef64(pn_encoder_t *encoder, uint64_t value) {
  if (pn_encoder_remaining(encoder) < 8) {
    return PN_OVERFLOW;
  } else {
    encoder->position[0] = 0xFF & (value >> 56);
    encoder->position[1] = 0xFF & (value >> 48);
    encoder->position[2] = 0xFF & (value >> 40);
    encoder->position[3] = 0xFF & (value >> 32);
    encoder->position[4] = 0xFF & (value >> 24);
    encoder->position[5] = 0xFF & (value >> 16);
    encoder->position[6] = 0xFF & (value >>  8);
    encoder->position[7] = 0xFF & (value      );
    encoder->position += 8;
    return 0;
  }
}

static inline int pn_encoder_writef128(pn_encoder_t *encoder, char *value) {
  if (pn_encoder_remaining(encoder) < 16) {
    return PN_OVERFLOW;
  } else {
    memmove(encoder->position, value, 16);
    encoder->position += 16;
    return 0;
  }
}

static inline int pn_encoder_writev8(pn_encoder_t *encoder, const pn_bytes_t *value)
{
  if (pn_encoder_remaining(encoder) < 1 + value->size) {
    return PN_OVERFLOW;
  } else {
    int e = pn_encoder_writef8(encoder, value->size);
    if (e) return e;
    memmove(encoder->position, value->start, value->size);
    encoder->position += value->size;
    return 0;
  }
}

static inline int pn_encoder_writev32(pn_encoder_t *encoder, const pn_bytes_t *value)
{
  if (pn_encoder_remaining(encoder) < 4 + value->size) {
    return PN_OVERFLOW;
  } else {
    int e = pn_encoder_writef32(encoder, value->size);
    if (e) return e;
    memmove(encoder->position, value->start, value->size);
    encoder->position += value->size;
    return 0;
  }
}

/* True if node is an element of an array - not the descriptor. */
static bool pn_is_in_array(pn_data_t *data, pni_node_t *parent, pni_node_t *node) {
  return (parent && parent->atom.type == PN_ARRAY) /* In array */
    && !(parent->described && !node->prev); /* Not the descriptor */
}

/** True if node is the first element of an array, not the descriptor.
 *@pre pn_is_in_array(data, parent, node)
 */
static bool pn_is_first_in_array(pn_data_t *data, pni_node_t *parent, pni_node_t *node) {
  if (!node->prev) return !parent->described; /* First node */
  return parent->described && (!pn_data_node(data, node->prev)->prev);
}

typedef union {
  uint32_t i;
  uint32_t a[2];
  uint64_t l;
  float f;
  double d;
} conv_t;

static int pni_encoder_enter(void *ctx, pn_data_t *data, pni_node_t *node)
{
  pn_encoder_t *encoder = (pn_encoder_t *) ctx;
  pni_node_t *parent = pn_data_node(data, node->parent);
  int err;
  pn_atom_t *atom = &node->atom;
  uint8_t code;
  conv_t c;

  /** In an array we don't write the code before each element, only the first. */
  if (pn_is_in_array(data, parent, node)) {
    code = pn_type2code(encoder, parent->type);
    if (pn_is_first_in_array(data, parent, node)) {
      err = pn_encoder_writef8(encoder, code);
      if (err) return err;
    }
  } else {
    code = pn_node2code(encoder, node);
    err = pn_encoder_writef8(encoder, code);
    if (err) return err;
  }

  switch (code) {
  case PNE_DESCRIPTOR:
  case PNE_NULL:
  case PNE_TRUE:
  case PNE_FALSE: return 0;
  case PNE_BOOLEAN: return pn_encoder_writef8(encoder, atom->u.as_bool);
  case PNE_UBYTE: return pn_encoder_writef8(encoder, atom->u.as_ubyte);
  case PNE_BYTE: return pn_encoder_writef8(encoder, atom->u.as_byte);
  case PNE_USHORT: return pn_encoder_writef16(encoder, atom->u.as_ushort);
  case PNE_SHORT: return pn_encoder_writef16(encoder, atom->u.as_short);
  case PNE_UINT0: return 0;
  case PNE_SMALLUINT: return pn_encoder_writef8(encoder, atom->u.as_uint);
  case PNE_UINT: return pn_encoder_writef32(encoder, atom->u.as_uint);
  case PNE_SMALLINT: return pn_encoder_writef8(encoder, atom->u.as_int);
  case PNE_INT: return pn_encoder_writef32(encoder, atom->u.as_int);
  case PNE_UTF32: return pn_encoder_writef32(encoder, atom->u.as_char);
  case PNE_ULONG: return pn_encoder_writef64(encoder, atom->u.as_ulong);
  case PNE_SMALLULONG: return pn_encoder_writef8(encoder, atom->u.as_ulong);
  case PNE_LONG: return pn_encoder_writef64(encoder, atom->u.as_long);
  case PNE_MS64: return pn_encoder_writef64(encoder, atom->u.as_timestamp);
  case PNE_FLOAT: c.f = atom->u.as_float; return pn_encoder_writef32(encoder, c.i);
  case PNE_DOUBLE: c.d = atom->u.as_double; return pn_encoder_writef64(encoder, c.l);
  case PNE_DECIMAL32: return pn_encoder_writef32(encoder, atom->u.as_decimal32);
  case PNE_DECIMAL64: return pn_encoder_writef64(encoder, atom->u.as_decimal64);
  case PNE_DECIMAL128: return pn_encoder_writef128(encoder, atom->u.as_decimal128.bytes);
  case PNE_UUID: return pn_encoder_writef128(encoder, atom->u.as_uuid.bytes);
  case PNE_VBIN8: return pn_encoder_writev8(encoder, &atom->u.as_bytes);
  case PNE_VBIN32: return pn_encoder_writev32(encoder, &atom->u.as_bytes);
  case PNE_STR8_UTF8: return pn_encoder_writev8(encoder, &atom->u.as_bytes);
  case PNE_STR32_UTF8: return pn_encoder_writev32(encoder, &atom->u.as_bytes);
  case PNE_SYM8: return pn_encoder_writev8(encoder, &atom->u.as_bytes);
  case PNE_SYM32: return pn_encoder_writev32(encoder, &atom->u.as_bytes);
  case PNE_ARRAY32:
    node->start = encoder->position;
    node->small = false;
    // we'll backfill the size on exit
    if (pn_encoder_remaining(encoder) < 4) return PN_OVERFLOW;
    encoder->position += 4;

    err = pn_encoder_writef32(encoder, node->described ? node->children - 1 : node->children);
    if (err) return err;

    if (node->described) {
      err = pn_encoder_writef8(encoder, 0);
      if (err) return err;
    }
    return 0;
  case PNE_LIST32:
  case PNE_MAP32:
    node->start = encoder->position;
    node->small = false;
    // we'll backfill the size later
    if (pn_encoder_remaining(encoder) < 4) return PN_OVERFLOW;
    encoder->position += 4;
    return pn_encoder_writef32(encoder, node->children);
  default:
    return pn_error_format(data->error, PN_ERR, "unrecognized encoding: %u", code);
  }
}

#include <stdio.h>

static int pni_encoder_exit(void *ctx, pn_data_t *data, pni_node_t *node)
{
  pn_encoder_t *encoder = (pn_encoder_t *) ctx;
  char *pos;
  int err;

  switch (node->atom.type) {
  case PN_ARRAY:
    if ((node->described && node->children == 1) ||
        (!node->described && node->children == 0)) {
      int err = pn_encoder_writef8(encoder, pn_type2code(encoder, node->type));
      if (err) return err;
    }
  case PN_LIST:
  case PN_MAP:
    pos = encoder->position;
    encoder->position = node->start;
    if (node->small) {
      // backfill size
      size_t size = pos - node->start - 1;
      err = pn_encoder_writef8(encoder, size);
    } else {
      // backfill size
      size_t size = pos - node->start - 4;
      err = pn_encoder_writef32(encoder, size);
    }
    encoder->position = pos;
    return err;
  default:
    return 0;
  }
}

ssize_t pn_encoder_encode(pn_encoder_t *encoder, pn_data_t *src, char *dst, size_t size)
{
  encoder->output = dst;
  encoder->position = dst;
  encoder->size = size;

  int err = pni_data_traverse(src, pni_encoder_enter, pni_encoder_exit, encoder);
  if (err) return err;

  return size - pn_encoder_remaining(encoder);
}
