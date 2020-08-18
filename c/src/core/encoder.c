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

static inline pn_error_t *pni_encoder_error(pn_encoder_t *encoder)
{
  if (!encoder->error) encoder->error = pn_error();
  return encoder->error;
}

void pn_encoder_initialize(pn_encoder_t *encoder)
{
  encoder->output = NULL;
  encoder->position = NULL;
  encoder->error = NULL;
  encoder->size = 0;
  encoder->null_count = 0;
}

void pn_encoder_finalize(pn_encoder_t *encoder)
{
  pn_error_free(encoder->error);
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
    return pn_error_format(pni_encoder_error(encoder), PN_ERR, "not a value type: %u\n", type);
  }
}

static uint8_t pn_node2code(pn_encoder_t *encoder, pni_node_t *node)
{
  switch (node->atom.type) {
  case PN_LONG:
    if (-128 <= node->atom.u.as_long && node->atom.u.as_long <= 127) {
      return PNE_SMALLLONG;
    } else {
      return PNE_LONG;
    }
  case PN_INT:
    if (-128 <= node->atom.u.as_int && node->atom.u.as_int <= 127) {
      return PNE_SMALLINT;
    } else {
      return PNE_INT;
    }
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

static size_t pn_encoder_remaining(pn_encoder_t *encoder) {
  char * end = encoder->output + encoder->size;
  if (end > encoder->position)
    return end - encoder->position;
  else
    return 0;
}

static inline void pn_encoder_writef8(pn_encoder_t *encoder, uint8_t value)
{
  if (pn_encoder_remaining(encoder)) {
    encoder->position[0] = value;
  }
  encoder->position++;
}

static inline void pn_encoder_writef16(pn_encoder_t *encoder, uint16_t value)
{
  if (pn_encoder_remaining(encoder) >= 2) {
    encoder->position[0] = 0xFF & (value >> 8);
    encoder->position[1] = 0xFF & (value     );
  }
  encoder->position += 2;
}

static inline void pn_encoder_writef32(pn_encoder_t *encoder, uint32_t value)
{
  if (pn_encoder_remaining(encoder) >= 4) {
    encoder->position[0] = 0xFF & (value >> 24);
    encoder->position[1] = 0xFF & (value >> 16);
    encoder->position[2] = 0xFF & (value >>  8);
    encoder->position[3] = 0xFF & (value      );
  }
  encoder->position += 4;
}

static inline void pn_encoder_writef64(pn_encoder_t *encoder, uint64_t value) {
  if (pn_encoder_remaining(encoder) >= 8) {
    encoder->position[0] = 0xFF & (value >> 56);
    encoder->position[1] = 0xFF & (value >> 48);
    encoder->position[2] = 0xFF & (value >> 40);
    encoder->position[3] = 0xFF & (value >> 32);
    encoder->position[4] = 0xFF & (value >> 24);
    encoder->position[5] = 0xFF & (value >> 16);
    encoder->position[6] = 0xFF & (value >>  8);
    encoder->position[7] = 0xFF & (value      );
  }
  encoder->position += 8;
}

static inline void pn_encoder_writef128(pn_encoder_t *encoder, char *value) {
  if (pn_encoder_remaining(encoder) >= 16) {
    memmove(encoder->position, value, 16);
  }
  encoder->position += 16;
}

static inline void pn_encoder_writev8(pn_encoder_t *encoder, const pn_bytes_t *value)
{
  pn_encoder_writef8(encoder, value->size);
  if (pn_encoder_remaining(encoder) >= value->size)
    memmove(encoder->position, value->start, value->size);
  encoder->position += value->size;
}

static inline void pn_encoder_writev32(pn_encoder_t *encoder, const pn_bytes_t *value)
{
  pn_encoder_writef32(encoder, value->size);
  if (pn_encoder_remaining(encoder) >= value->size)
    memmove(encoder->position, value->start, value->size);
  encoder->position += value->size;
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

/** True if node is in a described list - not the descriptor.
 *  - In this case we can omit trailing nulls
 */
static bool pn_is_in_described_list(pn_data_t *data, pni_node_t *parent, pni_node_t *node) {
  return parent && parent->atom.type == PN_LIST && parent->described;
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
  pn_atom_t *atom = &node->atom;
  uint8_t code;
  conv_t c;

  /** In an array we don't write the code before each element, only the first. */
  if (pn_is_in_array(data, parent, node)) {
    code = pn_type2code(encoder, parent->type);
    if (pn_is_first_in_array(data, parent, node)) {
      pn_encoder_writef8(encoder, code);
    }
  } else {
    code = pn_node2code(encoder, node);
    // Omit trailing nulls for described lists
    if (pn_is_in_described_list(data, parent, node)) {
      if (code==PNE_NULL) {
        encoder->null_count++;
      } else {
        // Output pending nulls, then the nodes code
        for (unsigned i = 0; i<encoder->null_count; i++) {
          pn_encoder_writef8(encoder, PNE_NULL);
        }
        encoder->null_count = 0;
        pn_encoder_writef8(encoder, code);
      }
    } else {
      pn_encoder_writef8(encoder, code);
    }
  }

  switch (code) {
  case PNE_DESCRIPTOR:
  case PNE_NULL:
  case PNE_TRUE:
  case PNE_FALSE: return 0;
  case PNE_BOOLEAN: pn_encoder_writef8(encoder, atom->u.as_bool); return 0;
  case PNE_UBYTE: pn_encoder_writef8(encoder, atom->u.as_ubyte); return 0;
  case PNE_BYTE: pn_encoder_writef8(encoder, atom->u.as_byte); return 0;
  case PNE_USHORT: pn_encoder_writef16(encoder, atom->u.as_ushort); return 0;
  case PNE_SHORT: pn_encoder_writef16(encoder, atom->u.as_short); return 0;
  case PNE_UINT0: return 0;
  case PNE_SMALLUINT: pn_encoder_writef8(encoder, atom->u.as_uint); return 0;
  case PNE_UINT: pn_encoder_writef32(encoder, atom->u.as_uint); return 0;
  case PNE_SMALLINT: pn_encoder_writef8(encoder, atom->u.as_int); return 0;
  case PNE_INT: pn_encoder_writef32(encoder, atom->u.as_int); return 0;
  case PNE_UTF32: pn_encoder_writef32(encoder, atom->u.as_char); return 0;
  case PNE_ULONG: pn_encoder_writef64(encoder, atom->u.as_ulong); return 0;
  case PNE_SMALLULONG: pn_encoder_writef8(encoder, atom->u.as_ulong); return 0;
  case PNE_LONG: pn_encoder_writef64(encoder, atom->u.as_long); return 0;
  case PNE_SMALLLONG: pn_encoder_writef8(encoder, atom->u.as_long); return 0;
  case PNE_MS64: pn_encoder_writef64(encoder, atom->u.as_timestamp); return 0;
  case PNE_FLOAT: c.f = atom->u.as_float; pn_encoder_writef32(encoder, c.i); return 0;
  case PNE_DOUBLE: c.d = atom->u.as_double; pn_encoder_writef64(encoder, c.l); return 0;
  case PNE_DECIMAL32: pn_encoder_writef32(encoder, atom->u.as_decimal32); return 0;
  case PNE_DECIMAL64: pn_encoder_writef64(encoder, atom->u.as_decimal64); return 0;
  case PNE_DECIMAL128: pn_encoder_writef128(encoder, atom->u.as_decimal128.bytes); return 0;
  case PNE_UUID: pn_encoder_writef128(encoder, atom->u.as_uuid.bytes); return 0;
  case PNE_VBIN8: pn_encoder_writev8(encoder, &atom->u.as_bytes); return 0;
  case PNE_VBIN32: pn_encoder_writev32(encoder, &atom->u.as_bytes); return 0;
  case PNE_STR8_UTF8: pn_encoder_writev8(encoder, &atom->u.as_bytes); return 0;
  case PNE_STR32_UTF8: pn_encoder_writev32(encoder, &atom->u.as_bytes); return 0;
  case PNE_SYM8: pn_encoder_writev8(encoder, &atom->u.as_bytes); return 0;
  case PNE_SYM32: pn_encoder_writev32(encoder, &atom->u.as_bytes); return 0;
  case PNE_ARRAY32:
    node->start = encoder->position;
    node->small = false;
    // we'll backfill the size on exit
    encoder->position += 4;
    pn_encoder_writef32(encoder, node->described ? node->children - 1 : node->children);
    if (node->described)
      pn_encoder_writef8(encoder, 0);
    return 0;
  case PNE_LIST32:
  case PNE_MAP32:
    node->start = encoder->position;
    node->small = false;
    // we'll backfill the size later
    encoder->position += 4;
    pn_encoder_writef32(encoder, node->children);
    return 0;
  default:
    return pn_error_format(pn_data_error(data), PN_ERR, "unrecognized encoding: %u", code);
  }
}

#include <stdio.h>

static int pni_encoder_exit(void *ctx, pn_data_t *data, pni_node_t *node)
{
  pn_encoder_t *encoder = (pn_encoder_t *) ctx;
  char *pos;

  // Special case 0 length list, but not as element in an array
  pni_node_t *parent = pn_data_node(data, node->parent);
  if (node->atom.type==PN_LIST && node->children-encoder->null_count==0 && !pn_is_in_array(data, parent, node)) {
    encoder->position = node->start-1; // position of list opcode
    pn_encoder_writef8(encoder, PNE_LIST0);
    encoder->null_count = 0;
    return 0;
  }

  switch (node->atom.type) {
  case PN_ARRAY:
    if ((node->described && node->children == 1) || (!node->described && node->children == 0)) {
      pn_encoder_writef8(encoder, pn_type2code(encoder, node->type));
    }
  // Fallthrough
  case PN_LIST:
  case PN_MAP:
    pos = encoder->position;
    encoder->position = node->start;
    if (node->small) {
      // backfill size
      size_t size = pos - node->start - 1;
      pn_encoder_writef8(encoder, size);
      // Adjust count
      if (encoder->null_count) {
        pn_encoder_writef8(encoder, node->children-encoder->null_count);
      }
    } else {
      // backfill size
      size_t size = pos - node->start - 4;
      pn_encoder_writef32(encoder, size);
      // Adjust count
      if (encoder->null_count) {
        pn_encoder_writef32(encoder, node->children-encoder->null_count);
      }
    }
    encoder->position = pos;
    encoder->null_count = 0;
    return 0;
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
  size_t encoded = encoder->position - encoder->output;
  if (encoded > size) {
      pn_error_format(pn_data_error(src), PN_OVERFLOW, "not enough space to encode");
      return PN_OVERFLOW;
  }
  return (ssize_t)encoded;
}

ssize_t pn_encoder_size(pn_encoder_t *encoder, pn_data_t *src)
{
  encoder->output = 0;
  encoder->position = 0;
  encoder->size = 0;

  pn_handle_t save = pn_data_point(src);
  int err = pni_data_traverse(src, pni_encoder_enter, pni_encoder_exit, encoder);
  pn_data_restore(src, save);

  if (err) return err;
  return encoder->position - encoder->output;
}
