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
#include <proton/codec.h>
#include "encodings.h"
#include "decoder.h"
#include "data.h"

#include <string.h>

/*
 * Decoding is iterative rather than recursive.
 *
 * Nesting depth is bounded only by the input, so a decoder that recursed once
 * per level would consume C stack in proportion to it. The state that a
 * recursive decoder would keep in its stack frames - how many children of the
 * enclosing container are still to come, and, for an array, the constructor
 * its elements share - is instead kept in the container's own node, in the
 * scratch space the encoder also uses (pni_decoder_state()).
 *
 * The tree being built is therefore also the decoder's stack: the only bound
 * on nesting is the node array, which is bounded by PNI_NID_MAX and by any
 * limit set with pn_data_set_decode_limits().
 */

void pn_decoder_initialize(pn_decoder_t *decoder)
{
  decoder->input = NULL;
  decoder->size = 0;
  decoder->position = NULL;
}

void pn_decoder_finalize(pn_decoder_t *decoder)
{
}

static inline uint8_t pn_decoder_readf8(pn_decoder_t *decoder)
{
  uint8_t r = decoder->position[0];
  decoder->position++;
  return r;
}

static inline uint16_t pn_decoder_readf16(pn_decoder_t *decoder)
{
  uint16_t a = (uint8_t) decoder->position[0];
  uint16_t b = (uint8_t) decoder->position[1];
  uint16_t r = a << 8
    | b;
  decoder->position += 2;
  return r;
}

static inline uint32_t pn_decoder_readf32(pn_decoder_t *decoder)
{
  uint32_t a = (uint8_t) decoder->position[0];
  uint32_t b = (uint8_t) decoder->position[1];
  uint32_t c = (uint8_t) decoder->position[2];
  uint32_t d = (uint8_t) decoder->position[3];
  uint32_t r = a << 24
    | b << 16
    | c <<  8
    | d;
  decoder->position += 4;
  return r;
}

static inline uint64_t pn_decoder_readf64(pn_decoder_t *decoder)
{
  uint64_t a = pn_decoder_readf32(decoder);
  uint64_t b = pn_decoder_readf32(decoder);
  return a << 32 | b;
}

static inline void pn_decoder_readf128(pn_decoder_t *decoder, void *dst)
{
  memmove(dst, decoder->position, 16);
  decoder->position += 16;
}

static inline size_t pn_decoder_remaining(pn_decoder_t *decoder)
{
  return decoder->input + decoder->size - decoder->position;
}

static inline pn_type_t pn_code2type(uint8_t code)
{
  switch (code)
  {
  case PNE_DESCRIPTOR:
    return (pn_type_t) PN_ARG_ERR;
  case PNE_NULL:
    return PN_NULL;
  case PNE_TRUE:
  case PNE_FALSE:
  case PNE_BOOLEAN:
    return PN_BOOL;
  case PNE_UBYTE:
    return PN_UBYTE;
  case PNE_BYTE:
    return PN_BYTE;
  case PNE_USHORT:
    return PN_USHORT;
  case PNE_SHORT:
    return PN_SHORT;
  case PNE_UINT0:
  case PNE_SMALLUINT:
  case PNE_UINT:
    return PN_UINT;
  case PNE_SMALLINT:
  case PNE_INT:
    return PN_INT;
  case PNE_UTF32:
    return PN_CHAR;
  case PNE_FLOAT:
    return PN_FLOAT;
  case PNE_LONG:
  case PNE_SMALLLONG:
    return PN_LONG;
  case PNE_MS64:
    return PN_TIMESTAMP;
  case PNE_DOUBLE:
    return PN_DOUBLE;
  case PNE_DECIMAL32:
    return PN_DECIMAL32;
  case PNE_DECIMAL64:
    return PN_DECIMAL64;
  case PNE_DECIMAL128:
    return PN_DECIMAL128;
  case PNE_UUID:
    return PN_UUID;
  case PNE_ULONG0:
  case PNE_SMALLULONG:
  case PNE_ULONG:
    return PN_ULONG;
  case PNE_VBIN8:
  case PNE_VBIN32:
    return PN_BINARY;
  case PNE_STR8_UTF8:
  case PNE_STR32_UTF8:
    return PN_STRING;
  case PNE_SYM8:
  case PNE_SYM32:
    return PN_SYMBOL;
  case PNE_LIST0:
  case PNE_LIST8:
  case PNE_LIST32:
    return PN_LIST;
  case PNE_ARRAY8:
  case PNE_ARRAY32:
    return PN_ARRAY;
  case PNE_MAP8:
  case PNE_MAP32:
    return PN_MAP;
  default:
    return (pn_type_t) PN_ARG_ERR;
  }
}

// Typecodes that introduce children which have to be decoded in turn.
// PNE_LIST0 is not one of them: it is an empty list, so it decodes in a single
// step like a scalar.
static inline bool pni_decoder_is_container_code(uint8_t code)
{
  switch (code)
  {
  case PNE_ARRAY8:
  case PNE_ARRAY32:
  case PNE_LIST8:
  case PNE_LIST32:
  case PNE_MAP8:
  case PNE_MAP32:
    return true;
  default:
    return false;
  }
}

// Everything else (bar the descriptor prefix) decodes to a single childless node.
static inline bool pni_decoder_is_scalar_code(uint8_t code)
{
  return code != PNE_DESCRIPTOR && !pni_decoder_is_container_code(code);
}

/*
 * The decoder's stack.
 *
 * "Open" nodes are the container and described nodes we have entered and not
 * yet left; depth counts them and is a local of pni_decoder_decode_value().
 * The innermost open node - the one we are putting children into - is
 * data->parent, and it carries the state saying what is left to decode.
 *
 * NB the returned pointer is invalidated by any pn_data_put_*(), which may
 * reallocate the node array; always re-fetch it after putting a node.
 */
static inline pni_decoder_state_t *pni_decoder_state(pn_data_t *data)
{
  return &pn_data_node(data, data->parent)->u.as_compound.scratch.as_decoder_state;
}

// One fewer child for the open node to wait for. Called before putting that
// child, so a node's count reaches 0 exactly as its last child is added.
static inline void pni_decoder_dec_remaining_children(pn_data_t *data, unsigned depth)
{
  if (depth > 0) pni_decoder_state(data)->remaining--;
}

// Array elements are encoded without a constructor of their own, so they are
// decoded differently from every other value.
static inline bool pni_decoder_in_array(pn_data_t *data, unsigned depth)
{
  if (depth == 0) return false;
  pn_type_t type = pni_data_parent_type(data);
  return type == PN_ARRAY || type == PN_ARRAY_DESCRIBED;
}

typedef union {
  uint32_t i;
  uint32_t a[2];
  uint64_t l;
  float f;
  double d;
} conv_t;

// Decode a value that has no children to decode: any scalar, or an empty list.
// The constructor has already been read; code is it.
static int pni_decoder_decode_scalar(pn_decoder_t *decoder, pn_data_t *data, uint8_t code)
{
  int err;
  conv_t conv;
  pn_decimal128_t dec128;
  pn_uuid_t uuid;
  size_t size;

  switch (code)
  {
  case PNE_NULL:
    err = pn_data_put_null(data);
    break;
  case PNE_TRUE:
    err = pn_data_put_bool(data, true);
    break;
  case PNE_FALSE:
    err = pn_data_put_bool(data, false);
    break;
  case PNE_BOOLEAN:
    if (!pn_decoder_remaining(decoder)) return PN_UNDERFLOW;
    err = pn_data_put_bool(data, pn_decoder_readf8(decoder) != 0);
    break;
  case PNE_UBYTE:
    if (!pn_decoder_remaining(decoder)) return PN_UNDERFLOW;
    err = pn_data_put_ubyte(data, pn_decoder_readf8(decoder));
    break;
  case PNE_BYTE:
    if (!pn_decoder_remaining(decoder)) return PN_UNDERFLOW;
    err = pn_data_put_byte(data, pn_decoder_readf8(decoder));
    break;
  case PNE_USHORT:
    if (pn_decoder_remaining(decoder) < 2) return PN_UNDERFLOW;
    err = pn_data_put_ushort(data, pn_decoder_readf16(decoder));
    break;
  case PNE_SHORT:
    if (pn_decoder_remaining(decoder) < 2) return PN_UNDERFLOW;
    err = pn_data_put_short(data, pn_decoder_readf16(decoder));
    break;
  case PNE_UINT:
    if (pn_decoder_remaining(decoder) < 4) return PN_UNDERFLOW;
    err = pn_data_put_uint(data, pn_decoder_readf32(decoder));
    break;
  case PNE_UINT0:
    err = pn_data_put_uint(data, 0);
    break;
  case PNE_SMALLUINT:
    if (!pn_decoder_remaining(decoder)) return PN_UNDERFLOW;
    err = pn_data_put_uint(data, pn_decoder_readf8(decoder));
    break;
  case PNE_SMALLINT:
    if (!pn_decoder_remaining(decoder)) return PN_UNDERFLOW;
    err = pn_data_put_int(data, (int8_t)pn_decoder_readf8(decoder));
    break;
  case PNE_INT:
    if (pn_decoder_remaining(decoder) < 4) return PN_UNDERFLOW;
    err = pn_data_put_int(data, pn_decoder_readf32(decoder));
    break;
  case PNE_UTF32:
    if (pn_decoder_remaining(decoder) < 4) return PN_UNDERFLOW;
    err = pn_data_put_char(data, pn_decoder_readf32(decoder));
    break;
  case PNE_FLOAT:
    if (pn_decoder_remaining(decoder) < 4) return PN_UNDERFLOW;
    // XXX: this assumes the platform uses IEEE floats
    conv.i = pn_decoder_readf32(decoder);
    err = pn_data_put_float(data, conv.f);
    break;
  case PNE_DECIMAL32:
    if (pn_decoder_remaining(decoder) < 4) return PN_UNDERFLOW;
    err = pn_data_put_decimal32(data, pn_decoder_readf32(decoder));
    break;
  case PNE_ULONG:
    if (pn_decoder_remaining(decoder) < 8) return PN_UNDERFLOW;
    err = pn_data_put_ulong(data, pn_decoder_readf64(decoder));
    break;
  case PNE_LONG:
    if (pn_decoder_remaining(decoder) < 8) return PN_UNDERFLOW;
    err = pn_data_put_long(data, pn_decoder_readf64(decoder));
    break;
  case PNE_MS64:
    if (pn_decoder_remaining(decoder) < 8) return PN_UNDERFLOW;
    err = pn_data_put_timestamp(data, pn_decoder_readf64(decoder));
    break;
  case PNE_DOUBLE:
    // XXX: this assumes the platform uses IEEE floats
    if (pn_decoder_remaining(decoder) < 8) return PN_UNDERFLOW;
    conv.l = pn_decoder_readf64(decoder);
    err = pn_data_put_double(data, conv.d);
    break;
  case PNE_DECIMAL64:
    if (pn_decoder_remaining(decoder) < 8) return PN_UNDERFLOW;
    err = pn_data_put_decimal64(data, pn_decoder_readf64(decoder));
    break;
  case PNE_ULONG0:
    err = pn_data_put_ulong(data, 0);
    break;
  case PNE_SMALLULONG:
    if (!pn_decoder_remaining(decoder)) return PN_UNDERFLOW;
    err = pn_data_put_ulong(data, pn_decoder_readf8(decoder));
    break;
  case PNE_SMALLLONG:
    if (!pn_decoder_remaining(decoder)) return PN_UNDERFLOW;
    err = pn_data_put_long(data, (int8_t)pn_decoder_readf8(decoder));
    break;
  case PNE_DECIMAL128:
    if (pn_decoder_remaining(decoder) < 16) return PN_UNDERFLOW;
    pn_decoder_readf128(decoder, &dec128);
    err = pn_data_put_decimal128(data, dec128);
    break;
  case PNE_UUID:
    if (pn_decoder_remaining(decoder) < 16) return PN_UNDERFLOW;
    pn_decoder_readf128(decoder, &uuid);
    err = pn_data_put_uuid(data, uuid);
    break;
  case PNE_VBIN8:
  case PNE_STR8_UTF8:
  case PNE_SYM8:
  case PNE_VBIN32:
  case PNE_STR32_UTF8:
  case PNE_SYM32:
    switch (code & 0xF0)
    {
    case 0xA0:
      if (!pn_decoder_remaining(decoder)) return PN_UNDERFLOW;
      size = pn_decoder_readf8(decoder);
      break;
    case 0xB0:
      if (pn_decoder_remaining(decoder) < 4) return PN_UNDERFLOW;
      size = pn_decoder_readf32(decoder);
      break;
    default:
      return pn_error_format(pn_data_error(data), PN_ARG_ERR, "unrecognized variable-width typecode: %u", code);
    }

    if (pn_decoder_remaining(decoder) < size) return PN_UNDERFLOW;

    {
      char *start = (char *) decoder->position;
      pn_bytes_t bytes = {size, start};
      switch (code & 0x0F)
      {
      case 0x0:
        err = pn_data_put_binary(data, bytes);
        break;
      case 0x1:
        err = pn_data_put_string(data, bytes);
        break;
      case 0x3:
        err = pn_data_put_symbol(data, bytes);
        break;
      default:
        return pn_error_format(pn_data_error(data), PN_ARG_ERR, "unrecognized variable-width typecode: %u", code);
      }
    }

    decoder->position += size;
    break;
  case PNE_LIST0:
    err = pn_data_put_list(data);
    break;
  default:
    return pn_error_format(pn_data_error(data), PN_ARG_ERR, "unrecognized typecode: %u", code);
  }

  return err;
}

// Decode the value of a descriptor. Descriptors are restricted to scalars: a
// compound descriptor buys nothing and is a nesting path we would rather not
// have to bound.
static int pni_decoder_decode_descriptor(pn_decoder_t *decoder, pn_data_t *data)
{
  if (!pn_decoder_remaining(decoder)) return PN_UNDERFLOW;

  uint8_t code = *decoder->position++;

  if (!pni_decoder_is_scalar_code(code)) {
    return pn_error_format(pn_data_error(data), PN_ARG_ERR, "invalid descriptor value typecode: %u", code);
  }

  return pni_decoder_decode_scalar(decoder, data, code);
}

// How many descriptors may prefix a single value: @d1:@d2:value is accepted,
// another level of chaining is not.
#define PNI_DECODER_MAX_DESCRIPTORS 2

/*
 * Read the constructor of the next value: the descriptors prefixing it, if
 * any, and then its format code, which is returned in *code.
 *
 * Each descriptor puts a PN_DESCRIBED node and enters it. Such a node holds
 * exactly two children: the descriptor value, decoded here, and the value it
 * describes. The node is left open for that value, which the caller decodes
 * and which closes the node.
 */
static int pni_decoder_decode_constructor(pn_decoder_t *decoder, pn_data_t *data,
                                          unsigned *depth, uint8_t *code)
{
  unsigned descriptors = 0;

  while (true) {
    if (!pn_decoder_remaining(decoder)) return PN_UNDERFLOW;

    uint8_t next = *decoder->position++;
    if (next != PNE_DESCRIPTOR) {
      *code = next;
      return 0;
    }

    if (++descriptors > PNI_DECODER_MAX_DESCRIPTORS) {
      return pn_error_format(pn_data_error(data), PN_ARG_ERR, "nested described type depth exceeded");
    }

    pni_decoder_dec_remaining_children(data, *depth);
    int err = pn_data_put_described(data);
    if (err) return err;
    pn_data_enter(data);
    (*depth)++;

    // Of the node's two children the descriptor is decoded right here, leaving
    // just the value it describes for the caller.
    pni_decoder_state(data)->remaining = 1;
    err = pni_decoder_decode_descriptor(decoder, data);
    if (err) return err;
  }
}

/*
 * Put an array node, enter it and read the constructor its elements share.
 *
 * An array may be prefixed by a descriptor, which describes the array as a
 * whole rather than any one element. It is held as the array node's first
 * child, so it is added before the element count is recorded and does not
 * count towards it.
 */
static int pni_decoder_open_array(pn_decoder_t *decoder, pn_data_t *data, unsigned *depth, pni_nid_t count)
{
  // The header check in pni_decoder_open_container leaves at least the one
  // constructor byte an array must have, so this peek is in bounds.
  bool described = (*decoder->position == PNE_DESCRIPTOR);

  int err = pn_data_put_array(data, described, (pn_type_t) 0);
  if (err) return err;
  pn_data_enter(data);
  (*depth)++;

  if (described) {
    decoder->position++;
    err = pni_decoder_decode_descriptor(decoder, data);
    if (err) return err;
  }

  if (!pn_decoder_remaining(decoder)) return PN_UNDERFLOW;
  uint8_t element_code = *decoder->position++;

  if (element_code == PNE_DESCRIPTOR) {
    return pn_error_format(pn_data_error(data), PN_ARG_ERR,
                           "chained descriptor not supported for a described array");
  }

  pn_type_t element_type = pn_code2type(element_code);
  if ((int) element_type < 0) {
    return pn_error_format(pn_data_error(data), (int) element_type,
                           "unrecognized array element typecode: %u", element_code);
  }
  // The array node had to be created before its element type could be known.
  pni_data_set_parent_array_type(data, element_type);

  pni_decoder_state_t *state = pni_decoder_state(data);
  state->remaining = count;
  state->typecode = element_code;
  return 0;
}

/*
 * Read a container header - the byte size and the child count - then put the
 * container node and enter it. Its children are decoded by the main loop; the
 * state left in the node says how many of them are still to come.
 */
static int pni_decoder_open_container(pn_decoder_t *decoder, pn_data_t *data, unsigned *depth, uint8_t code)
{
  const pn_type_t type = pn_code2type(code);  // PN_LIST, PN_MAP or PN_ARRAY
  size_t width;                               // bytes in each of the size and count fields

  switch (code)
  {
  case PNE_LIST8:  case PNE_MAP8:  case PNE_ARRAY8:  width = 1; break;
  case PNE_LIST32: case PNE_MAP32: case PNE_ARRAY32: width = 4; break;
  default:
    return pn_error_format(pn_data_error(data), PN_ARG_ERR, "internal error");
  }

  // What the size field has to cover: the count, and for an array at least one
  // byte of the constructor its elements share.
  const size_t min_size = width + (type == PN_ARRAY ? 1 : 0);

  if (pn_decoder_remaining(decoder) < width + min_size) return PN_UNDERFLOW;
  size_t size = (width == 1) ? pn_decoder_readf8(decoder) : pn_decoder_readf32(decoder);
  if (size < min_size) {
    return pn_error_format(pn_data_error(data), PN_ARG_ERR,
                           "%s size %zu too small to hold its own header",
                           pn_type_name(type), size);
  }
  if (pn_decoder_remaining(decoder) < size) return PN_UNDERFLOW;
  size_t count = (width == 1) ? pn_decoder_readf8(decoder) : pn_decoder_readf32(decoder);

  // Array elements of a zero width type (null, true, ...) take no input bytes
  // at all, so a count is not bounded by the size the way a list's is. Reject
  // any count that could never fit in this pn_data_t - whose node budget may be
  // well below the hard ceiling - rather than truncating it.
  if (count > pni_data_max_nid(data)) {
    return pn_error_format(pn_data_error(data), PN_OUT_OF_MEMORY,
                           "%s count %zu exceeds the pn_data node limit",
                           pn_type_name(type), count);
  }

  if (type == PN_ARRAY) return pni_decoder_open_array(decoder, data, depth, (pni_nid_t) count);

  int err = (type == PN_LIST) ? pn_data_put_list(data) : pn_data_put_map(data);
  if (err) return err;
  pn_data_enter(data);
  (*depth)++;
  pni_decoder_state(data)->remaining = (pni_nid_t) count;
  return 0;
}

// Decode one complete value - with all of its descendants - into data.
static int pni_decoder_decode_value(pn_decoder_t *decoder, pn_data_t *data)
{
  unsigned depth = 0;  // open nodes: how deep into the value we currently are

  while (true)
  {
    uint8_t code = 0;
    int err;

    if (pni_decoder_in_array(data, depth)) {
      code = pni_decoder_state(data)->typecode;  // every element shares it
    } else {
      err = pni_decoder_decode_constructor(decoder, data, &depth, &code);
      if (err) return err;
    }

    // Whatever we decode next is one of the children the open node is waiting for.
    pni_decoder_dec_remaining_children(data, depth);

    err = pni_decoder_is_container_code(code)
      ? pni_decoder_open_container(decoder, data, &depth, code)
      : pni_decoder_decode_scalar(decoder, data, code);
    if (err) return err;

    // Leave every node that now has all of its children - a container opened
    // empty is complete as soon as it is opened. Back at depth 0 the one value
    // we were asked for is complete.
    while (depth > 0 && pni_decoder_state(data)->remaining == 0) {
      pn_data_exit(data);
      depth--;
    }
    if (depth == 0) return 0;
  }
}

ssize_t pn_decoder_decode(pn_decoder_t *decoder, const char *src, size_t size, pn_data_t *dst)
{
  decoder->input = src;
  decoder->size = size;
  decoder->position = src;

  int err = pni_decoder_decode_value(decoder, dst);

  if (err == PN_UNDERFLOW)
      return pn_error_format(pn_data_error(dst), PN_UNDERFLOW, "not enough data to decode");
  if (err) return err;

  return decoder->position - decoder->input;
}
