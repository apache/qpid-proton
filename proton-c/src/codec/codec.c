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
