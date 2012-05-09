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

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <proton/codec.h>
#include <proton/util.h>
#include "encodings.h"

typedef union {
  uint32_t i;
  uint32_t a[2];
  uint64_t l;
  float f;
  double d;
} conv_t;

static int pn_write_code(char **pos, char *limit, uint8_t code) {
  char *dst = *pos;
  if (limit - dst < 1) {
    return PN_OVERFLOW;
  } else {
    dst[0] = code;
    *pos += 1;
    return 0;
  }
}
int pn_write_descriptor(char **pos, char *limit) {
  return pn_write_code(pos, limit, PNE_DESCRIPTOR);
}
int pn_write_null(char **pos, char *limit) {
  return pn_write_code(pos, limit, PNE_NULL);
}

static int pn_write_fixed8(char **pos, char *limit, uint8_t v, uint8_t code) {
  char *dst = *pos;
  if (limit - dst < 2) {
    return PN_OVERFLOW;
  } else {
    dst[0] = code;
    dst[1] = v;
    *pos += 2;
    return 0;
  }
}

int pn_write_boolean(char **pos, char *limit, bool v) {
  return pn_write_fixed8(pos, limit, v, PNE_BOOLEAN);
}
int pn_write_ubyte(char **pos, char *limit, uint8_t v) {
  return pn_write_fixed8(pos, limit, v, PNE_UBYTE);
}
int pn_write_byte(char **pos, char *limit, int8_t v) {
  return pn_write_fixed8(pos, limit, v, PNE_BYTE);
}

static int pn_write_fixed16(char **pos, char *limit, uint16_t v,
                            uint8_t code) {
  char *dst = *pos;
  if (limit - dst < 3) {
    return PN_OVERFLOW;
  } else {
    dst[0] = code;
    *((uint16_t *) (dst + 1)) = htons(v);
    *pos += 3;
    return 0;
  }
}
int pn_write_ushort(char **pos, char *limit, uint16_t v) {
  return pn_write_fixed16(pos, limit, v, PNE_USHORT);
}
int pn_write_short(char **pos, char *limit, int16_t v) {
  return pn_write_fixed16(pos, limit, v, PNE_SHORT);
}

static int pn_write_fixed32(char **pos, char *limit, uint32_t v, uint8_t code) {
  char *dst = *pos;
  if (limit - dst < 5) {
    return PN_OVERFLOW;
  } else {
    dst[0] = code;
    *((uint32_t *) (dst + 1)) = htonl(v);
    *pos += 5;
    return 0;
  }
}
int pn_write_uint(char **pos, char *limit, uint32_t v) {
  return pn_write_fixed32(pos, limit, v, PNE_UINT);
}
int pn_write_int(char **pos, char *limit, int32_t v) {
  return pn_write_fixed32(pos, limit, v, PNE_INT);
}
int pn_write_char(char **pos, char *limit, wchar_t v) {
  return pn_write_fixed32(pos, limit, v, PNE_UTF32);
}
int pn_write_float(char **pos, char *limit, float v) {
  conv_t c;
  c.f = v;
  return pn_write_fixed32(pos, limit, c.i, PNE_FLOAT);
}

static int pn_write_fixed64(char **pos, char *limit, uint64_t v, uint8_t code) {
  char *dst = *pos;
  if (limit - dst < 9) {
    return PN_OVERFLOW;
  } else {
    dst[0] = code;
    uint32_t hi = v >> 32;
    uint32_t lo = v;
    *((uint32_t *) (dst + 1)) = htonl(hi);
    *((uint32_t *) (dst + 5)) = htonl(lo);
    *pos += 9;
    return 0;
  }
}
int pn_write_ulong(char **pos, char *limit, uint64_t v) {
  return pn_write_fixed64(pos, limit, v, PNE_ULONG);
}
int pn_write_long(char **pos, char *limit, int64_t v) {
  return pn_write_fixed64(pos, limit, v, PNE_LONG);
}
int pn_write_double(char **pos, char *limit, double v) {
  conv_t c;
  c.d = v;
  return pn_write_fixed64(pos, limit, c.l, PNE_DOUBLE);
}

#define CONSISTENT (1)

static int pn_write_variable(char **pos, char *limit, size_t size, const char *src,
                             uint8_t code8, uint8_t code32) {
  int n;

  if (!CONSISTENT && size < 256) {
    if ((n = pn_write_fixed8(pos, limit, size, code8)))
      return n;
  } else {
    if ((n = pn_write_fixed32(pos, limit, size, code32)))
      return n;
  }

  if (limit - *pos < size) return PN_OVERFLOW;

  memmove(*pos, src, size);
  *pos += size;
  return 0;
}
int pn_write_binary(char **pos, char *limit, size_t size, const char *src) {
  return pn_write_variable(pos, limit, size, src, PNE_VBIN8, PNE_VBIN32);
}
int pn_write_utf8(char **pos, char *limit, size_t size, char *utf8) {
  return pn_write_variable(pos, limit, size, utf8, PNE_STR8_UTF8, PNE_STR32_UTF8);
}
int pn_write_symbol(char **pos, char *limit, size_t size, const char *symbol) {
  return pn_write_variable(pos, limit, size, (char *) symbol, PNE_SYM8, PNE_SYM32);
}

int pn_write_start(char **pos, char *limit, char **start) {
  char *dst = *pos;
  if (limit - dst < 9) {
    return PN_OVERFLOW;
  } else {
    *start = dst;
    *pos += 9;
    return 0;
  }
}

static int pn_write_end(char **pos, char *limit, char *start, size_t count, uint8_t code) {
  int n;
  if ((n = pn_write_fixed32(&start, limit, *pos - start - 5, code)))
    return n;
  *((uint32_t *) start) = htonl(count);
  return 0;
}

int pn_write_list(char **pos, char *limit, char *start, size_t count) {
  return pn_write_end(pos, limit, start, count, PNE_LIST32);
}

int pn_write_map(char **pos, char *limit, char *start, size_t count) {
  return pn_write_end(pos, limit, start, 2*count, PNE_MAP32);
}

ssize_t pn_read_datum(const char *bytes, size_t n, pn_data_callbacks_t *cb, void *ctx);

ssize_t pn_read_type(const char *bytes, size_t n, pn_data_callbacks_t *cb, void *ctx, uint8_t *code)
{
  if (bytes[0] != PNE_DESCRIPTOR) {
    *code = bytes[0];
    return 1;
  } else {
    ssize_t offset = 1;
    ssize_t rcode;
    cb->start_descriptor(ctx);
    rcode = pn_read_datum(bytes + offset, n - offset, cb, ctx);
    cb->stop_descriptor(ctx);
    if (rcode < 0) return rcode;
    offset += rcode;
    rcode = pn_read_type(bytes + offset, n - offset, cb, ctx, code);
    if (rcode < 0) return rcode;
    offset += rcode;
    return offset;
  }
}

ssize_t pn_read_encoding(const char *bytes, size_t n, pn_data_callbacks_t *cb, void *ctx, uint8_t code)
{
  size_t size;
  size_t count;
  conv_t conv;
  ssize_t rcode;
  int offset = 0;

  switch (code)
  {
  case PNE_DESCRIPTOR:
    return PN_ARG_ERR;
  case PNE_NULL:
    cb->on_null(ctx);
    return offset;
  case PNE_TRUE:
    cb->on_bool(ctx, true);
    return offset;
  case PNE_FALSE:
    cb->on_bool(ctx, false);
    return offset;
  case PNE_BOOLEAN:
    cb->on_bool(ctx, *(bytes + offset) != 0);
    offset += 1;
    return offset;
  case PNE_UBYTE:
    cb->on_ubyte(ctx, *((uint8_t *) (bytes + offset)));
    offset += 1;
    return offset;
  case PNE_BYTE:
    cb->on_byte(ctx, *((int8_t *) (bytes + offset)));
    offset += 1;
    return offset;
  case PNE_USHORT:
    cb->on_ushort(ctx, ntohs(*((uint16_t *) (bytes + offset))));
    offset += 2;
    return offset;
  case PNE_SHORT:
    cb->on_short(ctx, (int16_t) ntohs(*((int16_t *) (bytes + offset))));
    offset += 2;
    return offset;
  case PNE_UINT:
    cb->on_uint(ctx, ntohl(*((uint32_t *) (bytes + offset))));
    offset += 4;
    return offset;
  case PNE_UINT0:
    cb->on_uint(ctx, 0);
    return offset;
  case PNE_SMALLUINT:
    cb->on_uint(ctx, *((uint8_t *) (bytes + offset)));
    offset += 1;
    return offset;
  case PNE_INT:
    cb->on_int(ctx, ntohl(*((uint32_t *) (bytes + offset))));
    offset += 4;
    return offset;
  case PNE_FLOAT:
    // XXX: this assumes the platform uses IEEE floats
    conv.i = ntohl(*((uint32_t *) (bytes + offset)));
    cb->on_float(ctx, conv.f);
    offset += 4;
    return offset;
  case PNE_ULONG:
  case PNE_LONG:
  case PNE_DOUBLE:
    {
      uint32_t hi = ntohl(*((uint32_t *) (bytes + offset)));
      offset += 4;
      uint32_t lo = ntohl(*((uint32_t *) (bytes + offset)));
      offset += 4;
      conv.l = (((uint64_t) hi) << 32) | lo;
    }

    switch (code)
    {
    case PNE_ULONG:
      cb->on_ulong(ctx, conv.l);
      break;
    case PNE_LONG:
      cb->on_long(ctx, (int64_t) conv.l);
      break;
    case PNE_DOUBLE:
      // XXX: this assumes the platform uses IEEE floats
      cb->on_double(ctx, conv.d);
      break;
    default:
      return PN_ARG_ERR;
    }

    return offset;
  case PNE_ULONG0:
    cb->on_ulong(ctx, 0);
    return offset;
  case PNE_SMALLULONG:
    cb->on_ulong(ctx, *((uint8_t *) (bytes + offset)));
    offset += 1;
    return offset;
  case PNE_VBIN8:
  case PNE_STR8_UTF8:
  case PNE_SYM8:
  case PNE_VBIN32:
  case PNE_STR32_UTF8:
  case PNE_SYM32:
    switch (code & 0xF0)
    {
    case 0xA0:
      size = *(uint8_t *) (bytes + offset);
      offset += 1;
      break;
    case 0xB0:
      size = ntohl(*(uint32_t *) (bytes + offset));
      offset += 4;
      break;
    default:
      return PN_ARG_ERR;
    }

    {
      char *start = (char *) (bytes + offset);
      switch (code & 0x0F)
      {
      case 0x0:
        cb->on_binary(ctx, size, start);
        break;
      case 0x1:
        cb->on_utf8(ctx, size, start);
        break;
      case 0x3:
        cb->on_symbol(ctx, size, start);
        break;
      default:
        return PN_ARG_ERR;
      }
    }

    offset += size;
    return offset;
  case PNE_LIST0:
    count = 0;
    cb->start_list(ctx, count);
    cb->stop_list(ctx, count);
    return offset;
  case PNE_ARRAY8:
  case PNE_ARRAY32:
  case PNE_LIST8:
  case PNE_LIST32:
  case PNE_MAP8:
  case PNE_MAP32:
    switch (code)
    {
    case PNE_ARRAY8:
    case PNE_LIST8:
    case PNE_MAP8:
      size = *(uint8_t *) (bytes + offset);
      offset += 1;
      count = *(uint8_t *) (bytes + offset);
      offset += 1;
      break;
    case PNE_ARRAY32:
    case PNE_LIST32:
    case PNE_MAP32:
      size = ntohl(*(uint32_t *) (bytes + offset));
      offset += 4;
      count = ntohl(*(uint32_t *) (bytes + offset));
      offset += 4;
      break;
    default:
      return PN_ARG_ERR;
    }

    switch (code)
    {
    case PNE_ARRAY8:
    case PNE_ARRAY32:
      {
        uint8_t acode;
        rcode = pn_read_type(bytes + offset, n - offset, cb, ctx, &acode);
        cb->start_array(ctx, count, acode);
        if (rcode < 0) return rcode;
        offset += rcode;
        for (int i = 0; i < count; i++)
        {
          rcode = pn_read_encoding(bytes + offset, n - offset, cb, ctx, acode);
          if (rcode < 0) return rcode;
          offset += rcode;
        }
        cb->stop_array(ctx, count, acode);
      }
      return offset;
    case PNE_LIST8:
    case PNE_LIST32:
      cb->start_list(ctx, count);
      break;
    case PNE_MAP8:
    case PNE_MAP32:
      cb->start_map(ctx, count);
      break;
    default:
      return PN_ARG_ERR;
    }

    for (int i = 0; i < count; i++)
    {
      rcode = pn_read_datum(bytes + offset, n - offset, cb, ctx);
      if (rcode < 0) return rcode;
      offset += rcode;
    }

    switch (code)
    {
    case PNE_LIST8:
    case PNE_LIST32:
      cb->stop_list(ctx, count);
      break;
    case PNE_MAP8:
    case PNE_MAP32:
      cb->stop_map(ctx, count);
      break;
    default:
      return PN_ARG_ERR;
    }

    return offset;
  default:
    printf("Unrecognised typecode: %u\n", code);
    return PN_ARG_ERR;
  }
}

ssize_t pn_read_datum(const char *bytes, size_t n, pn_data_callbacks_t *cb, void *ctx)
{
  uint8_t code;
  ssize_t rcode;
  size_t offset = 0;

  rcode = pn_read_type(bytes + offset, n - offset, cb, ctx, &code);
  if (rcode < 0) return rcode;
  offset += rcode;
  rcode = pn_read_encoding(bytes + offset, n - offset, cb, ctx, code);
  if (rcode < 0) return rcode;
  offset += rcode;
  return offset;
}

void noop_null(void *ctx) {}
void noop_bool(void *ctx, bool v) {}
void noop_ubyte(void *ctx, uint8_t v) {}
void noop_byte(void *ctx, int8_t v) {}
void noop_ushort(void *ctx, uint16_t v) {}
void noop_short(void *ctx, int16_t v) {}
void noop_uint(void *ctx, uint32_t v) {}
void noop_int(void *ctx, int32_t v) {}
void noop_float(void *ctx, float f) {}
void noop_ulong(void *ctx, uint64_t v) {}
void noop_long(void *ctx, int64_t v) {}
void noop_double(void *ctx, double v) {}
void noop_binary(void *ctx, size_t size, char *bytes) {}
void noop_utf8(void *ctx, size_t size, char *bytes) {}
void noop_symbol(void *ctx, size_t size, char *bytes) {}
void noop_start_array(void *ctx, size_t count, uint8_t code) {}
void noop_stop_array(void *ctx, size_t count, uint8_t code) {}
void noop_start_list(void *ctx, size_t count) {}
void noop_stop_list(void *ctx, size_t count) {}
void noop_start_map(void *ctx, size_t count) {}
void noop_stop_map(void *ctx, size_t count) {}
void noop_start_descriptor(void *ctx) {}
void noop_stop_descriptor(void *ctx) {}

pn_data_callbacks_t *noop = &PN_DATA_CALLBACKS(noop);

void print_null(void *ctx) { printf("null\n"); }
void print_bool(void *ctx, bool v) { if (v) printf("true\n"); else printf("false\n"); }
void print_ubyte(void *ctx, uint8_t v) { printf("%hhu\n", v); }
void print_byte(void *ctx, int8_t v) { printf("%hhi\n", v); }
void print_ushort(void *ctx, uint16_t v) { printf("%hu\n", v); }
void print_short(void *ctx, int16_t v) { printf("%hi\n", v); }
void print_uint(void *ctx, uint32_t v) { printf("%u\n", v); }
void print_int(void *ctx, int32_t v) { printf("%i\n", v); }
void print_float(void *ctx, float v) { printf("%f\n", v); }
void print_ulong(void *ctx, uint64_t v) { printf("%"PRIu64"\n", v); }
void print_long(void *ctx, int64_t v) { printf("%"PRIi64"\n", v); }
void print_double(void *ctx, double v) { printf("%f\n", v); }

void print_bytes(char *label, int size, char *bytes) {
  printf("%s(%.*s)\n", label, size, bytes);
}

void print_binary(void *ctx, size_t size, char *bytes) { print_bytes("bin", size, bytes); }
void print_utf8(void *ctx, size_t size, char *bytes) { print_bytes("utf8", size, bytes); }
void print_symbol(void *ctx, size_t size, char *bytes) { print_bytes("sym", size, bytes); }
void print_start_array(void *ctx, size_t count, uint8_t code) { printf("start array %zd\n", count); }
void print_stop_array(void *ctx, size_t count, uint8_t code) { printf("stop array %zd\n", count); }
void print_start_list(void *ctx, size_t count) { printf("start list %zd\n", count); }
void print_stop_list(void *ctx, size_t count) { printf("stop list %zd\n", count); }
void print_start_map(void *ctx, size_t count) { printf("start map %zd\n", count); }
void print_stop_map(void *ctx, size_t count) { printf("stop map %zd\n", count); }
void print_start_descriptor(void *ctx) { printf("start descriptor "); }
void print_stop_descriptor(void *ctx) { printf("stop descriptor "); }

pn_data_callbacks_t *printer = &PN_DATA_CALLBACKS(print);

// new codec
#include "../util.h"

const char *pn_type_str(pn_type_t type)
{
  switch (type)
  {
  case PN_NULL: return "PN_NULL";
  case PN_BOOL: return "PN_BOOL";
  case PN_UBYTE: return "PN_UBYTE";
  case PN_BYTE: return "PN_BYTE";
  case PN_USHORT: return "PN_USHORT";
  case PN_SHORT: return "PN_SHORT";
  case PN_UINT: return "PN_UINT";
  case PN_INT: return "PN_INT";
  case PN_ULONG: return "PN_ULONG";
  case PN_LONG: return "PN_LONG";
  case PN_FLOAT: return "PN_FLOAT";
  case PN_DOUBLE: return "PN_DOUBLE";
  case PN_BINARY: return "PN_BINARY";
  case PN_STRING: return "PN_STRING";
  case PN_SYMBOL: return "PN_SYMBOL";
  case PN_DESCRIPTOR: return "PN_DESCRIPTOR";
  case PN_ARRAY: return "PN_ARRAY";
  case PN_LIST: return "PN_LIST";
  case PN_MAP: return "PN_MAP";
  case PN_TYPE: return "PN_TYPE";
  }

  return "<UNKNOWN>";
}

void pn_print_datum(pn_datum_t datum)
{
  switch (datum.type)
  {
  case PN_NULL:
    printf("null");
    break;
  case PN_BOOL:
    printf(datum.u.as_bool ? "true" : "false");
    break;
  case PN_UBYTE:
    printf("%u", datum.u.as_ubyte);
    break;
  case PN_BYTE:
    printf("%i", datum.u.as_byte);
    break;
  case PN_USHORT:
    printf("%u", datum.u.as_ushort);
    break;
  case PN_SHORT:
    printf("%i", datum.u.as_short);
    break;
  case PN_UINT:
    printf("%u", datum.u.as_uint);
    break;
  case PN_INT:
    printf("%i", datum.u.as_int);
    break;
  case PN_ULONG:
    printf("%lu", datum.u.as_ulong);
    break;
  case PN_LONG:
    printf("%li", datum.u.as_long);
    break;
  case PN_FLOAT:
    printf("%f", datum.u.as_float);
    break;
  case PN_DOUBLE:
    printf("%f", datum.u.as_double);
    break;
  case PN_BINARY:
    pn_print_data(datum.u.as_binary.start, datum.u.as_binary.size);
    break;
  case PN_STRING:
    printf("%.*s", (int) datum.u.as_string.size, datum.u.as_string.start);
    break;
  case PN_SYMBOL:
    printf("%.*s", (int) datum.u.as_symbol.size, datum.u.as_symbol.start);
    break;
  case PN_DESCRIPTOR:
    printf("descriptor");
    break;
  case PN_ARRAY:
    printf("array[%zu]", datum.u.count);
    break;
  case PN_LIST:
    printf("list[%zu]", datum.u.count);
    break;
  case PN_MAP:
    printf("map[%zu]", datum.u.count);
    break;
  case PN_TYPE:
    printf("%s", pn_type_str(datum.u.type));
    break;
  }
}

void pn_print_indent(int level)
{
  for (int i = 0; i < level; i++)
  {
    printf("  ");
  }
}

size_t pn_bytes_ltrim(pn_bytes_t *bytes, size_t size)
{
  if (size > bytes->size)
    size = bytes->size;

  bytes->start += size;
  bytes->size -= size;
  return size;
}

size_t pn_data_ltrim(pn_data_t *data, size_t size)
{
  if (size > data->size)
    size = data->size;

  data->start += size;
  data->size -= size;

  return size;
}

int pn_pprint_data_one(pn_data_t *data, int level)
{
  if (!data->size) return PN_UNDERFLOW;
  pn_datum_t *datum = data->start;
  pn_data_ltrim(data, 1);
  int err, count;

  switch (datum->type) {
  case PN_DESCRIPTOR:
    err = pn_pprint_data_one(data, level + 1);
    if (err) return err;
    printf("(");
    err = pn_pprint_data_one(data, level + 1);
    if (err) return err;
    printf(")");
    return 0;
  case PN_ARRAY:
    count = datum->u.count;
    printf("@");
    err = pn_pprint_data_one(data, level + 1);
    if (err) return err;
    printf("[");
    for (int i = 0; i < count; i++)
    {
      err = pn_pprint_data_one(data, level + 1);
      if (err) return err;
      if (i < count - 1) printf(", ");
    }
    printf("]");
    return 0;
  case PN_LIST:
  case PN_MAP:
    count = datum->u.count;
    bool list = datum->type == PN_LIST;
    printf("%s",  list ? "[" : "{");
    for (int i = 0; i < count; i++)
    {
      err = pn_pprint_data_one(data, level + 1);
      if (err) return err;
      if (list) {
        if (i < count - 1) printf(", ");
      } else {
        if (i % 2) {
          if (i < count - 1) printf(", ");
        } else {
          printf(": ");
        }
      }
    }
    printf("%s",  list ? "]" : "}");
    return 0;
  default:
    pn_print_datum(*datum);
    return 0;
  }
}

int pn_pprint_data(const pn_data_t *data)
{
  pn_data_t copy = *data;
  int err;

  while (copy.size) {
    err = pn_pprint_data_one(&copy, 0);
    if (err) return err;
    if (copy.size) printf(" ");
  }

  return 0;
}

int pn_decode_datum(pn_bytes_t *bytes, pn_data_t *data);
int pn_encode_datum(pn_bytes_t *bytes, pn_data_t *data);

int pn_decode_data(pn_bytes_t *bytes, pn_data_t *data)
{
  pn_bytes_t buf = *bytes;
  pn_data_t dat = *data;

  while (buf.size) {
    int e = pn_decode_datum(&buf, &dat);
    if (e) return e;
  }

  data->size -= dat.size;

  return 0;
}

int pn_encode_data(pn_bytes_t *bytes, pn_data_t *data)
{
  pn_bytes_t buf = *bytes;
  pn_data_t dat = *data;

  while (dat.size) {
    int e = pn_encode_datum(&buf, &dat);
    if (e) return e;
  }

  bytes->size -= buf.size;

  return 0;
}

uint8_t pn_type2code(pn_type_t type)
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
  case PN_FLOAT: return PNE_FLOAT;
  case PN_LONG: return PNE_LONG;
  case PN_DOUBLE: return PNE_DOUBLE;
  case PN_ULONG: return PNE_ULONG;
  case PN_BINARY: return PNE_VBIN32;
  case PN_STRING: return PNE_STR32_UTF8;
  case PN_SYMBOL: return PNE_SYM32;
  case PN_LIST: return PNE_LIST32;
  case PN_ARRAY: return PNE_ARRAY32;
  case PN_MAP: return PNE_MAP32;
  default:
    pn_fatal("not a value type: %u", type);
    return 0;
  }
}

int pn_encode_type(pn_bytes_t *bytes, pn_data_t *data, pn_type_t *type);
int pn_encode_value(pn_bytes_t *bytes, pn_data_t *data, pn_type_t type);

int pn_encode_datum(pn_bytes_t *bytes, pn_data_t *data)
{
  pn_type_t type;
  int e = pn_encode_type(bytes, data, &type);
  if (e) return e;
  return pn_encode_value(bytes, data, type);
}

int pn_bytes_writef8(pn_bytes_t *bytes, uint8_t value)
{
  if (bytes->size) {
    bytes->start[0] = value;
    pn_bytes_ltrim(bytes, 1);
    return 0;
  } else {
    return PN_OVERFLOW;
  }
}

int pn_bytes_writef16(pn_bytes_t *bytes, uint16_t value)
{
  if (bytes->size < 2) {
    return PN_OVERFLOW;
  } else {
    *((uint16_t *) (bytes->start)) = htons(value);
    pn_bytes_ltrim(bytes, 2);
    return 0;
  }
}

int pn_bytes_writef32(pn_bytes_t *bytes, uint32_t value)
{
  if (bytes->size < 4) {
    return PN_OVERFLOW;
  } else {
    *((uint32_t *) (bytes->start)) = htonl(value);
    pn_bytes_ltrim(bytes, 4);
    return 0;
  }
}

int pn_bytes_writef64(pn_bytes_t *bytes, uint64_t value) {
  if (bytes->size < 8) {
    return PN_OVERFLOW;
  } else {
    uint32_t hi = value >> 32;
    uint32_t lo = value;
    *((uint32_t *) (bytes->start)) = htonl(hi);
    *((uint32_t *) (bytes->start + 4)) = htonl(lo);
    pn_bytes_ltrim(bytes, 8);
    return 0;
  }
}

int pn_bytes_writev32(pn_bytes_t *bytes, const pn_bytes_t *value)
{
  if (bytes->size < 4 + value->size) {
    return PN_OVERFLOW;
  } else {
    int e = pn_bytes_writef32(bytes, value->size);
    if (e) return e;
    memmove(bytes->start, value->start, value->size);
    pn_bytes_ltrim(bytes, value->size);
    return 0;
  }
}

int pn_encode_type(pn_bytes_t *bytes, pn_data_t *data, pn_type_t *type)
{
  if (!data->size) return PN_UNDERFLOW;

  pn_datum_t *datum = data->start;
  if (datum->type == PN_DESCRIPTOR)
  {
    pn_data_ltrim(data, 1);
    int e = pn_bytes_writef8(bytes, 0);
    if (e) return e;
    e = pn_encode_datum(bytes, data);
    if (e) return e;
    return pn_encode_type(bytes, data, type);
  } else if (datum->type == PN_TYPE) {
    *type = datum->u.type;
  } else {
    *type = datum->type;
  }

  return pn_bytes_writef8(bytes, pn_type2code(*type));
}

int pn_encode_value(pn_bytes_t *bytes, pn_data_t *data, pn_type_t type)
{
  pn_datum_t *datum = data->start;
  int e;
  conv_t c;

  if (!data->size) return PN_UNDERFLOW;
  pn_data_ltrim(data, 1);

  switch (type)
  {
  case PN_NULL: return 0;
  case PN_BOOL: return pn_bytes_writef8(bytes, datum->u.as_bool);
  case PN_UBYTE: return pn_bytes_writef8(bytes, datum->u.as_ubyte);
  case PN_BYTE: return pn_bytes_writef8(bytes, datum->u.as_byte);
  case PN_USHORT: return pn_bytes_writef16(bytes, datum->u.as_ushort);
  case PN_SHORT: return pn_bytes_writef16(bytes, datum->u.as_short);
  case PN_UINT: return pn_bytes_writef32(bytes, datum->u.as_uint);
  case PN_INT: return pn_bytes_writef32(bytes, datum->u.as_int);
  case PN_ULONG: return pn_bytes_writef64(bytes, datum->u.as_ulong);
  case PN_LONG: return pn_bytes_writef64(bytes, datum->u.as_long);
  case PN_FLOAT: c.f = datum->u.as_float; return pn_bytes_writef32(bytes, c.i);
  case PN_DOUBLE: c.d = datum->u.as_double; return pn_bytes_writef32(bytes, c.l);
  case PN_BINARY: return pn_bytes_writev32(bytes, &datum->u.as_binary);
  case PN_STRING: return pn_bytes_writev32(bytes, &datum->u.as_string);
  case PN_SYMBOL: return pn_bytes_writev32(bytes, &datum->u.as_symbol);
  case PN_ARRAY:
    {
      char *start = bytes->start;
      // we'll backfill the size later
      if (bytes->size < 4) return PN_OVERFLOW;
      pn_bytes_ltrim(bytes, 4);
      size_t count = datum->u.count;
      e = pn_bytes_writef32(bytes, count);
      if (e) return e;
      pn_type_t atype;
      e = pn_encode_type(bytes, data, &atype);
      if (e) return e;
      // trim the type
      pn_data_ltrim(data, 1);

      for (int i = 0; i < count; i++)
      {
        e = pn_encode_value(bytes, data, atype);
        if (e) return e;
      }

      // backfill size
      size_t size = bytes->start - start - 4;
      pn_bytes_t size_bytes = {4, start};
      pn_bytes_writef32(&size_bytes, size);
      return 0;
    }
  case PN_LIST:
  case PN_MAP:
    {
      char *start = bytes->start;
      // we'll backfill the size later
      if (bytes->size < 4) return PN_OVERFLOW;
      pn_bytes_ltrim(bytes, 4);
      size_t count = datum->u.count;
      e = pn_bytes_writef32(bytes, count);
      if (e) return e;

      for (int i = 0; i < count; i++)
      {
        e = pn_encode_datum(bytes, data);
        if (e) return e;
      }

      // backfill size
      size_t size = bytes->start - start - 4;
      pn_bytes_t size_bytes = {4, start};
      pn_bytes_writef32(&size_bytes, size);
      return 0;
    }
  default:
    pn_fatal("datum has no value: %u", datum->u.type);
    return PN_ARG_ERR;
  }
}

int pn_decode_type(pn_bytes_t *bytes, pn_data_t *data, uint8_t *code)
{
  if (!bytes->size) return PN_UNDERFLOW;
  if (!data->size) return PN_OVERFLOW;

  if (bytes->start[0] != PNE_DESCRIPTOR) {
    *code = bytes->start[0];
    pn_bytes_ltrim(bytes, 1);
    return 0;
  } else {
    data->start[0] = (pn_datum_t) {.type=PN_DESCRIPTOR};
    pn_bytes_ltrim(bytes, 1);
    pn_data_ltrim(data, 1);
    int e = pn_decode_datum(bytes, data);
    if (e) return e;
    e = pn_decode_type(bytes, data, code);
    return e;
  }
}

pn_type_t pn_code2type(uint8_t code)
{
  switch (code)
  {
  case PNE_DESCRIPTOR:
    return PN_ARG_ERR;
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
  case PNE_INT:
    return PN_INT;
  case PNE_FLOAT:
    return PN_FLOAT;
  case PNE_LONG:
    return PN_LONG;
  case PNE_DOUBLE:
    return PN_DOUBLE;
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
    printf("Unrecognised typecode: %u\n", code);
    return PN_ARG_ERR;
  }
}

int pn_decode_value(pn_bytes_t *bytes, pn_data_t *data, uint8_t code)
{
  size_t size;
  size_t count;
  conv_t conv;

  if (!data->size) return PN_OVERFLOW;

  pn_datum_t datum;

  switch (code)
  {
  case PNE_DESCRIPTOR:
    return PN_ARG_ERR;
  case PNE_NULL:
    datum.type=PN_NULL;
    break;
  case PNE_TRUE:
    datum.type=PN_BOOL, datum.u.as_bool=true;
    break;
  case PNE_FALSE:
    datum.type=PN_BOOL, datum.u.as_bool=false;
    break;
  case PNE_BOOLEAN:
    if (!bytes->size) return PN_UNDERFLOW;
    datum.type=PN_BOOL, datum.u.as_bool=(*(bytes->start) != 0);
    pn_bytes_ltrim(bytes, 1);
    break;
  case PNE_UBYTE:
    if (!bytes->size) return PN_UNDERFLOW;
    datum.type=PN_UBYTE, datum.u.as_ubyte=*((uint8_t *) (bytes->start));
    pn_bytes_ltrim(bytes, 1);
    break;
  case PNE_BYTE:
    if (!bytes->size) return PN_UNDERFLOW;
    datum.type=PN_BYTE, datum.u.as_byte=*((int8_t *) (bytes->start));
    pn_bytes_ltrim(bytes, 1);
    break;
  case PNE_USHORT:
    if (bytes->size < 2) return PN_UNDERFLOW;
    datum.type=PN_USHORT, datum.u.as_ushort=ntohs(*((uint16_t *) (bytes->start)));
    pn_bytes_ltrim(bytes, 2);
    break;
  case PNE_SHORT:
    if (bytes->size < 2) return PN_UNDERFLOW;
    datum.type=PN_SHORT, datum.u.as_short=ntohs(*((int16_t *) (bytes->start)));
    pn_bytes_ltrim(bytes, 2);
    break;
  case PNE_UINT:
    if (bytes->size < 4) return PN_UNDERFLOW;
    datum.type=PN_UINT, datum.u.as_uint=ntohl(*((uint32_t *) (bytes->start)));
    pn_bytes_ltrim(bytes, 4);
    break;
  case PNE_UINT0:
    datum.type=PN_UINT, datum.u.as_uint=0;
    break;
  case PNE_SMALLUINT:
    if (!bytes->size) return PN_UNDERFLOW;
    datum.type=PN_UINT, datum.u.as_uint=*((uint8_t *) (bytes->start));
    pn_bytes_ltrim(bytes, 1);
    break;
  case PNE_INT:
    if (bytes->size < 4) return PN_UNDERFLOW;
    datum.type=PN_INT, datum.u.as_int=ntohl(*((uint32_t *) (bytes->start)));
    pn_bytes_ltrim(bytes, 4);
    break;
  case PNE_FLOAT:
    if (bytes->size < 4) return PN_UNDERFLOW;
    // XXX: this assumes the platform uses IEEE floats
    conv.i = ntohl(*((uint32_t *) (bytes->start)));
    datum.type=PN_FLOAT, datum.u.as_float=conv.f;
    pn_bytes_ltrim(bytes, 4);
    break;
  case PNE_ULONG:
  case PNE_LONG:
  case PNE_DOUBLE:
    if (bytes->size < 8) return PN_UNDERFLOW;

    {
      uint32_t hi = ntohl(*((uint32_t *) (bytes->start)));
      uint32_t lo = ntohl(*((uint32_t *) (bytes->start + 4)));
      conv.l = (((uint64_t) hi) << 32) | lo;
    }

    switch (code)
    {
    case PNE_ULONG:
      datum.type=PN_ULONG, datum.u.as_ulong=conv.l;
      break;
    case PNE_LONG:
      datum.type=PN_LONG, datum.u.as_long=(int64_t) conv.l;
      break;
    case PNE_DOUBLE:
      // XXX: this assumes the platform uses IEEE floats
      datum.type=PN_DOUBLE, datum.u.as_double=conv.d;
      break;
    default:
      return PN_ARG_ERR;
    }

    pn_bytes_ltrim(bytes, 8);
    break;
  case PNE_ULONG0:
    datum.type=PN_ULONG, datum.u.as_ulong=0;
    break;
  case PNE_SMALLULONG:
    if (!bytes->size) return PN_UNDERFLOW;
    datum.type=PN_ULONG, datum.u.as_ulong=*((uint8_t *) (bytes->start));
    pn_bytes_ltrim(bytes, 1);
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
      if (!bytes->size) return PN_UNDERFLOW;
      size = *(uint8_t *) (bytes->start);
      pn_bytes_ltrim(bytes, 1);
      break;
    case 0xB0:
      if (bytes->size < 4) return PN_UNDERFLOW;
      size = ntohl(*(uint32_t *) (bytes->start));
      pn_bytes_ltrim(bytes, 4);
      break;
    default:
      return PN_ARG_ERR;
    }

    {
      char *start = (char *) (bytes->start);
      pn_bytes_t binary = {.size=size, .start=start};
      switch (code & 0x0F)
      {
      case 0x0:
        datum.type=PN_BINARY, datum.u.as_binary=binary;
        break;
      case 0x1:
        datum.type=PN_STRING, datum.u.as_binary=binary;
        break;
      case 0x3:
        datum.type=PN_SYMBOL, datum.u.as_binary=binary;
        break;
      default:
        return PN_ARG_ERR;
      }
    }

    if (bytes->size < size) return PN_UNDERFLOW;
    pn_bytes_ltrim(bytes, size);
    break;
  case PNE_LIST0:
    datum.type=PN_LIST, datum.u.count=0;
    break;
  case PNE_ARRAY8:
  case PNE_ARRAY32:
  case PNE_LIST8:
  case PNE_LIST32:
  case PNE_MAP8:
  case PNE_MAP32:
    switch (code)
    {
    case PNE_ARRAY8:
    case PNE_LIST8:
    case PNE_MAP8:
      if (bytes->size < 2) return PN_UNDERFLOW;
      size = *(uint8_t *) (bytes->start);
      count = *(uint8_t *) (bytes->start + 1);
      pn_bytes_ltrim(bytes, 2);
      break;
    case PNE_ARRAY32:
    case PNE_LIST32:
    case PNE_MAP32:
      size = ntohl(*(uint32_t *) (bytes->start));
      count = ntohl(*(uint32_t *) (bytes->start + 4));
      pn_bytes_ltrim(bytes, 8);
      break;
    default:
      return PN_ARG_ERR;
    }

    switch (code)
    {
    case PNE_ARRAY8:
    case PNE_ARRAY32:
      {
        if (!data->size) return PN_OVERFLOW;
        data->start[0] = (pn_datum_t) {.type=PN_ARRAY, .u.count=count};
        pn_data_ltrim(data, 1);
        uint8_t acode;
        int e = pn_decode_type(bytes, data, &acode);
        if (e) return e;
        if (!data->size) return PN_OVERFLOW;
        pn_type_t type = pn_code2type(acode);
        if (type < 0) return type;
        data->start[0] = (pn_datum_t) {.type=PN_TYPE, .u.type=type};
        pn_data_ltrim(data, 1);
        for (int i = 0; i < count; i++)
        {
          e = pn_decode_value(bytes, data, acode);
          if (e) return e;
        }
      }
      return 0;
    case PNE_LIST8:
    case PNE_LIST32:
      if (!data->size) return PN_OVERFLOW;
      data->start[0] = (pn_datum_t) {.type=PN_LIST, .u.count=count};
      pn_data_ltrim(data, 1);
      break;
    case PNE_MAP8:
    case PNE_MAP32:
      if (!data->size) return PN_OVERFLOW;
      data->start[0] = (pn_datum_t) {.type=PN_MAP, .u.count=count};
      pn_data_ltrim(data, 1);
      break;
    default:
      return PN_ARG_ERR;
    }

    for (int i = 0; i < count; i++)
    {
      int e = pn_decode_datum(bytes, data);
      if (e) return e;
    }

    return 0;
  default:
    printf("Unrecognised typecode: %u\n", code);
    return PN_ARG_ERR;
  }

  if (!data->size) return PN_OVERFLOW;
  data->start[0] = datum;
  pn_data_ltrim(data, 1);

  return 0;
}

int pn_decode_datum(pn_bytes_t *bytes, pn_data_t *data)
{
  uint8_t code;
  int e;

  if ((e = pn_decode_type(bytes, data, &code))) return e;
  if ((e = pn_decode_value(bytes, data, code))) return e;

  return 0;
}

int pn_vfill_one(pn_data_t *data, const char **fmt, va_list ap)
{
  int err, count;
  char code = **fmt;
  if (!code) return PN_ARG_ERR;
  if (!data->size) return PN_OVERFLOW;
  pn_datum_t *datum = data->start;
  pn_data_ltrim(data, 1);
  (*fmt)++;

  switch (code) {
  case 'n':
    *datum = (pn_datum_t) {PN_NULL, {0}};
    return 0;
  case 'o':
    datum->type = PN_BOOL;
    datum->u.as_bool = va_arg(ap, int);
    return 0;
  case 'B':
    datum->type = PN_UBYTE;
    datum->u.as_ubyte = va_arg(ap, unsigned int);
    return 0;
  case 'b':
    datum->type = PN_BYTE;
    datum->u.as_byte = va_arg(ap, int);
    return 0;
  case 'H':
    datum->type = PN_USHORT;
    datum->u.as_ushort = va_arg(ap, unsigned int);
    return 0;
  case 'h':
    datum->type = PN_SHORT;
    datum->u.as_short = va_arg(ap, int);
    return 0;
  case 'I':
    datum->type = PN_UINT;
    datum->u.as_uint = va_arg(ap, uint32_t);
    return 0;
  case 'i':
    datum->type = PN_INT;
    datum->u.as_int = va_arg(ap, int32_t);
    return 0;
  case 'L':
    datum->type = PN_ULONG;
    datum->u.as_ulong = va_arg(ap, uint64_t);
    return 0;
  case 'l':
    datum->type = PN_LONG;
    datum->u.as_long = va_arg(ap, int64_t);
    return 0;
  case 'f':
    datum->type = PN_FLOAT;
    datum->u.as_float = va_arg(ap, double);
    return 0;
  case 'd':
    datum->type = PN_DOUBLE;
    datum->u.as_double = va_arg(ap, double);
    return 0;
  case 'z':
    datum->type = PN_BINARY;
    datum->u.as_binary.size = va_arg(ap, size_t);
    datum->u.as_binary.start = va_arg(ap, char *);
    return 0;
  case 'S':
    datum->type = PN_STRING;
    datum->u.as_string.start = va_arg(ap, char *);
    datum->u.as_string.size = strlen(datum->u.as_string.start);
    return 0;
  case 's':
    datum->type = PN_SYMBOL;
    datum->u.as_symbol.start = va_arg(ap, char *);
    datum->u.as_symbol.size = strlen(datum->u.as_symbol.start);
    return 0;
  case 'D':
    *datum = (pn_datum_t) {PN_DESCRIPTOR, {0}};
    err = pn_vfill_one(data, fmt, ap);
    if (err) return err;
    err = pn_vfill_one(data, fmt, ap);
    if (err) return err;
    return 0;
  case 'T':
    datum->type = PN_TYPE;
    datum->u.type = va_arg(ap, int);
    return 0;
  case '@':
    datum->type = PN_ARRAY;
    err = pn_vfill_one(data, fmt, ap);
    if (err) return err;
    if (data->start[-1].type != PN_TYPE) {
      fprintf(stderr, "expected PN_TYPE, got %s\n", pn_type_str(data->start[-1].type));
      return PN_ARG_ERR;
    }
    if (**fmt != '[') {
      fprintf(stderr, "expected '['\n");
      return PN_ARG_ERR;
    }
    (*fmt)++;
    count = 0;
    while (**fmt && **fmt != ']') {
      err = pn_vfill_one(data, fmt, ap);
      if (err) return err;
      count++;
    }
    if (**fmt != ']') {
      fprintf(stderr, "expected ']'\n");
      return PN_ARG_ERR;
    }
    (*fmt)++;
    datum->u.count = count;
    return 0;
  case '[':
    datum->type = PN_LIST;
    count = 0;
    while (**fmt && **fmt != ']') {
      err = pn_vfill_one(data, fmt, ap);
      if (err) return err;
      count++;
    }
    if (**fmt != ']') {
      fprintf(stderr, "expected ']'\n");
      return PN_ARG_ERR;
    }
    (*fmt)++;
    datum->u.count = count;
    return 0;
  case '{':
    datum->type = PN_MAP;
    count = 0;
    while (**fmt && **fmt != '}') {
      err = pn_vfill_one(data, fmt, ap);
      if (err) return err;
      count++;
    }
    if (**fmt != '}') {
      fprintf(stderr, "expected '}'\n");
      return PN_ARG_ERR;
    }
    (*fmt)++;
    datum->u.count = count;
    return 0;
  default:
    fprintf(stderr, "unrecognized code: '%c'\n", code);
    return PN_ARG_ERR;
  }
}

int pn_fill_data(pn_data_t *data, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  const char *pos = fmt;
  pn_data_t dbuf = *data;

  while (*pos) {
    int err = pn_vfill_one(&dbuf, &pos, ap);
    if (err) {
      va_end(ap);
      return err;
    }
  }
  va_end(ap);

  data->size -= dbuf.size;
  return 0;
}

int pn_skip(pn_data_t *data, size_t n)
{
  for (int i = 0; i < n; i++) {
    if (!data->size) return PN_UNDERFLOW;
    pn_datum_t *datum = data->start;
    pn_data_ltrim(data, 1);
    int err;

    switch (datum->type)
    {
    case PN_DESCRIPTOR:
      err = pn_skip(data, 2);
      if (err) return err;
      break;
    case PN_ARRAY:
      err = pn_skip(data, 1);
      if (err) return err;
      err = pn_skip(data, datum->u.count);
      if (err) return err;
      break;
    case PN_LIST:
    case PN_MAP:
      err = pn_skip(data, datum->u.count);
      if (err) return err;
      break;
    default:
      break;
    }
  }

  return 0;
}

int pn_scan_one(pn_data_t *data, const char **fmt, va_list ap, bool *scanned)
{
  char code = **fmt;
  pn_datum_t *datum = data ? data->start : NULL;
  (*fmt)++;
  size_t count;
  int err;

  switch (code) {
  case 'n':
    if (datum && datum->type == PN_NULL) {
      *scanned = true;
    } else {
      *scanned = false;
    }
    if (data) pn_data_ltrim(data, 1);
    return 0;
  case 'o':
    {
      bool *value = va_arg(ap, bool *);
      if (datum && datum->type == PN_BOOL) {
        *value = datum->u.as_bool;
        *scanned = true;
      } else {
        *value = 0;
        *scanned = false;
      }
    }
    if (data) pn_data_ltrim(data, 1);
    return 0;
  case 'B':
    {
      uint8_t *value = va_arg(ap, uint8_t *);
      if (datum && datum->type == PN_UBYTE) {
        *value = datum->u.as_ubyte;
        *scanned = true;
      } else {
        *value = 0;
        *scanned = false;
      }
    }
    if (data) pn_data_ltrim(data, 1);
    return 0;
  case 'b':
    {
      int8_t *value = va_arg(ap, int8_t *);
      if (datum && datum->type == PN_BYTE) {
        *value = datum->u.as_byte;
        *scanned = true;
      } else {
        *value = 0;
        *scanned = false;
      }
    }
    if (data) pn_data_ltrim(data, 1);
    return 0;
  case 'H':
    {
      uint16_t *value = va_arg(ap, uint16_t *);
      if (datum && datum->type == PN_USHORT) {
        *value = datum->u.as_ushort;
        *scanned = true;
      } else {
        *value = 0;
        *scanned = false;
      }
    }
    if (data) pn_data_ltrim(data, 1);
    return 0;
  case 'h':
    {
      int16_t *value = va_arg(ap, int16_t *);
      if (datum && datum->type == PN_SHORT) {
        *value = datum->u.as_short;
        *scanned = true;
      } else {
        *value = 0;
        *scanned = false;
      }
    }
    if (data) pn_data_ltrim(data, 1);
    return 0;
  case 'I':
    {
      uint32_t *value = va_arg(ap, uint32_t *);
      if (datum && datum->type == PN_UINT) {
        *value = datum->u.as_uint;
        *scanned = true;
      } else {
        *value = 0;
        *scanned = false;
      }
    }
    if (data) pn_data_ltrim(data, 1);
    return 0;
  case 'i':
    {
      int32_t *value = va_arg(ap, int32_t *);
      if (datum && datum->type == PN_INT) {
        *value = datum->u.as_int;
        *scanned = true;
      } else {
        *value = 0;
        *scanned = false;
      }
    }
    if (data) pn_data_ltrim(data, 1);
    return 0;
  case 'L':
    {
      uint64_t *value = va_arg(ap, uint64_t *);
      if (datum && datum->type == PN_ULONG) {
        *value = datum->u.as_ulong;
        *scanned = true;
      } else {
        *value = 0;
        *scanned = false;
      }
    }
    if (data) pn_data_ltrim(data, 1);
    return 0;
  case 'l':
    {
      int64_t *value = va_arg(ap, int64_t *);
      if (datum && datum->type == PN_LONG) {
        *value = datum->u.as_long;
        *scanned = true;
      } else {
        *value = 0;
        *scanned = false;
      }
    }
    if (data) pn_data_ltrim(data, 1);
    return 0;
  case 'f':
    {
      float *value = va_arg(ap, float *);
      if (datum && datum->type == PN_FLOAT) {
        *value = datum->u.as_float;
        *scanned = true;
      } else {
        *value = 0;
        *scanned = false;
      }
    }
    if (data) pn_data_ltrim(data, 1);
    return 0;
  case 'd':
    {
      double *value = va_arg(ap, double *);
      if (datum && datum->type == PN_DOUBLE) {
        *value = datum->u.as_double;
        *scanned = true;
      } else {
        *value = 0;
        *scanned = false;
      }
    }
    if (data) pn_data_ltrim(data, 1);
    return 0;
  case 'z':
    {
      pn_bytes_t *bytes = va_arg(ap, pn_bytes_t *);
      if (datum && datum->type == PN_BINARY) {
        *bytes = datum->u.as_binary;
        *scanned = true;
      } else {
        *bytes = (pn_bytes_t) {0, 0};
        *scanned = false;
      }
    }
    if (data) pn_data_ltrim(data, 1);
    return 0;
  case 'S':
    {
      pn_bytes_t *bytes = va_arg(ap, pn_bytes_t *);
      if (datum && datum->type == PN_STRING) {
        *bytes = datum->u.as_string;
        *scanned = true;
      } else {
        *bytes = (pn_bytes_t) {0, 0};
        *scanned = false;
      }
    }
    if (data) pn_data_ltrim(data, 1);
    return 0;
  case 's':
    {
      pn_bytes_t *bytes = va_arg(ap, pn_bytes_t *);
      if (datum && datum->type == PN_SYMBOL) {
        *bytes = datum->u.as_symbol;
        *scanned = true;
      } else {
        *bytes = (pn_bytes_t) {0, 0};
        *scanned = false;
      }
    }
    if (data) pn_data_ltrim(data, 1);
    return 0;
  case 'D':
    if (data) pn_data_ltrim(data, 1);
    if (datum && datum->type == PN_DESCRIPTOR) {
      bool scd, scv;
      err = pn_scan_one(data, fmt, ap, &scd);
      if (err) return err;
      err = pn_scan_one(data, fmt, ap, &scv);
      if (err) return err;
      *scanned = scd && scv;
    } else {
      *scanned = false;
    }
    return 0;
  case 'T':
    if (datum && datum->type == PN_TYPE) {
      pn_type_t *type = va_arg(ap, pn_type_t *);
      *type = datum->u.type;
      *scanned = true;
    } else {
      *scanned = false;
    }
    if (data) pn_data_ltrim(data, 1);
    return 0;
  case '@':
    if (data) pn_data_ltrim(data, 1);
    if (datum && datum->type == PN_ARRAY) {
      bool sc;
      err = pn_scan_one(data, fmt, ap, &sc);
      if (err) return err;
      if (**fmt != '[') {
        fprintf(stderr, "expected '['\n");
        return PN_ARG_ERR;
      }
      (*fmt)++;
      count = 0;
      while (**fmt && **fmt != ']') {
        bool sce;
        if (count < datum->u.count) {
          err = pn_scan_one(data, fmt, ap, &sce);
          if (err) return err;
          sc = sc && sce;
        } else {
          err = pn_scan_one(NULL, fmt, ap, &sce);
          if (err) return err;
          sc = sc && sce;
        }
        count++;
      }
      if (**fmt != ']') {
        fprintf(stderr, "expected ']'\n");
        return PN_ARG_ERR;
      }
      (*fmt)++;
      if (count < datum->u.count) {
        err = pn_skip(data, datum->u.count - count);
        if (err) return err;
      }
      *scanned = (datum->u.count == count) && sc;
    } else {
      *scanned = false;
    }
    return 0;
  case '[':
    if (data) pn_data_ltrim(data, 1);
    if (datum && datum->type == PN_LIST) {
      count = 0;
      bool sc = true;
      while (**fmt && **fmt != ']') {
        bool sce;
        if (count < datum->u.count) {
          err = pn_scan_one(data, fmt, ap, &sce);
          if (err) return err;
          sc = sc && sce;
        } else {
          err = pn_scan_one(NULL, fmt, ap, &sce);
          if (err) return err;
          sc = sc && sce;
        }
        count++;
      }
      if (**fmt != ']') {
        fprintf(stderr, "expected ']'\n");
        return PN_ARG_ERR;
      }
      (*fmt)++;
      if (count < datum->u.count) {
        err = pn_skip(data, datum->u.count - count);
        if (err) return err;
      }
      *scanned = (datum->u.count == count) && sc;
    } else {
      *scanned = false;
    }
    return 0;
  case '{':
    if (data) pn_data_ltrim(data, 1);
    if (datum && datum->type == PN_MAP) {
      count = 0;
      bool sc = true;
      while (count < datum->u.count && **fmt && **fmt != '}') {
        bool sce;
        if (count < datum->u.count) {
          err = pn_scan_one(data, fmt, ap, &sce);
          if (err) return err;
          sc = sc && sce;
        } else {
          err = pn_scan_one(NULL, fmt, ap, &sce);
          if (err) return err;
          sc = sc && sce;
        }
        count++;
      }
      if (**fmt != '}') {
        fprintf(stderr, "expected '}'\n");
        return PN_ARG_ERR;
      }
      (*fmt)++;
      if (count < datum->u.count) {
        err = pn_skip(data, datum->u.count - count);
        if (err) return err;
      }
      *scanned = (datum->u.count == count) && sc;
    } else {
      *scanned = false;
    }
    return 0;
  case '.':
    err = pn_skip(data, 1);
    if (err) return err;
    *scanned = true;
    return 0;
  case '?':
    {
      bool *sc = va_arg(ap, bool *);
      err = pn_scan_one(data, fmt, ap, sc);
      if (err) return err;
      *scanned = true;
    }
    return 0;
  default:
    fprintf(stderr, "unrecognized code: '%c'\n", code);
    return PN_ARG_ERR;
  }
}

int pn_scan_data(const pn_data_t *data, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  pn_data_t dbuf = *data;
  const char *pos = fmt;
  bool scanned;

  while (*pos) {
    int err = pn_scan_one(&dbuf, &pos, ap, &scanned);
    if (err) {
      va_end(ap);
      return err;
    }
  }

  va_end(ap);

  return 0;
}
