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
#include "../util.h"

typedef union {
  uint32_t i;
  uint32_t a[2];
  uint64_t l;
  float f;
  double d;
} conv_t;

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

void pn_print_datum(pn_datum_t datum);

size_t pn_bytes_ltrim(pn_bytes_t *bytes, size_t size);

int pn_bytes_format(pn_bytes_t *bytes, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(bytes->start, bytes->size, fmt, ap);
  va_end(ap);
  if (n >= bytes->size) {
    return PN_OVERFLOW;
  } else if (n > 0) {
    pn_bytes_ltrim(bytes, n);
    return 0;
  } else {
    return PN_ERR;
  }
}

int pn_format_datum(pn_bytes_t *bytes, pn_datum_t datum)
{
  switch (datum.type)
  {
  case PN_NULL:
    return pn_bytes_format(bytes, "null");
  case PN_BOOL:
    return pn_bytes_format(bytes, datum.u.as_bool ? "true" : "false");
  case PN_UBYTE:
    return pn_bytes_format(bytes, "%u", datum.u.as_ubyte);
  case PN_BYTE:
    return pn_bytes_format(bytes, "%i", datum.u.as_byte);
  case PN_USHORT:
    return pn_bytes_format(bytes, "%u", datum.u.as_ushort);
  case PN_SHORT:
    return pn_bytes_format(bytes, "%i", datum.u.as_short);
  case PN_UINT:
    return pn_bytes_format(bytes, "%u", datum.u.as_uint);
  case PN_INT:
    return pn_bytes_format(bytes, "%i", datum.u.as_int);
  case PN_ULONG:
    return pn_bytes_format(bytes, "%lu", datum.u.as_ulong);
  case PN_LONG:
    return pn_bytes_format(bytes, "%li", datum.u.as_long);
  case PN_FLOAT:
    return pn_bytes_format(bytes, "%f", datum.u.as_float);
  case PN_DOUBLE:
    return pn_bytes_format(bytes, "%f", datum.u.as_double);
  case PN_BINARY:
  case PN_STRING:
  case PN_SYMBOL:
    {
      int err;
      const char *pfx;
      pn_bytes_t bin;
      switch (datum.type) {
      case PN_BINARY:
        pfx = "b";
        bin = datum.u.as_binary;
        break;
      case PN_STRING:
        pfx = "u";
        bin = datum.u.as_string;
        break;
      case PN_SYMBOL:
        pfx = "";
        bin = datum.u.as_symbol;
        break;
      default:
        pn_fatal("XXX");
        return PN_ERR;
      }

      if ((err = pn_bytes_format(bytes, "%s\"", pfx))) return err;
      ssize_t n = pn_quote_data(bytes->start, bytes->size, bin.start, bin.size);
      if (n < 0) return n;
      pn_bytes_ltrim(bytes, n);
      return pn_bytes_format(bytes, "\"");
    }
  case PN_DESCRIPTOR:
    return pn_bytes_format(bytes, "descriptor");
  case PN_ARRAY:
    return pn_bytes_format(bytes, "array[%zu]", datum.u.count);
  case PN_LIST:
    return pn_bytes_format(bytes, "list[%zu]", datum.u.count);
  case PN_MAP:
    return pn_bytes_format(bytes, "map[%zu]", datum.u.count);
  case PN_TYPE:
    return pn_bytes_format(bytes, "%s", pn_type_str(datum.u.type));
  }

  return PN_ARG_ERR;
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

int pn_format_data_one(pn_bytes_t *bytes, pn_data_t *data, int level)
{
  if (!data->size) return PN_UNDERFLOW;
  pn_datum_t *datum = data->start;
  pn_data_ltrim(data, 1);
  int err, count;

  switch (datum->type) {
  case PN_DESCRIPTOR:
    if ((err = pn_format_data_one(bytes, data, level + 1))) return err;
    if ((err = pn_bytes_format(bytes, "("))) return err;
    if ((err = pn_format_data_one(bytes, data, level + 1))) return err;
    if ((err = pn_bytes_format(bytes, ")"))) return err;
    return 0;
  case PN_ARRAY:
    count = datum->u.count;
    if ((err = pn_bytes_format(bytes, "@"))) return err;
    if ((err = pn_format_data_one(bytes, data, level + 1))) return err;
    if ((err = pn_bytes_format(bytes, "["))) return err;
    for (int i = 0; i < count; i++)
    {
      if ((err = pn_format_data_one(bytes, data, level + 1))) return err;
      if (i < count - 1) {
        if ((err = pn_bytes_format(bytes, ", "))) return err;
      }
    }
    if ((err = pn_bytes_format(bytes, "]"))) return err;
    return 0;
  case PN_LIST:
  case PN_MAP:
    count = datum->u.count;
    bool list = datum->type == PN_LIST;
    if ((err = pn_bytes_format(bytes, "%s",  list ? "[" : "{"))) return err;
    for (int i = 0; i < count; i++)
    {
      if ((err = pn_format_data_one(bytes, data, level + 1))) return err;
      if (list) {
        if (i < count - 1) {
          if ((err = pn_bytes_format(bytes, ", "))) return err;
        }
      } else {
        if (i % 2) {
          if (i < count - 1) {
            if ((err = pn_bytes_format(bytes, ", "))) return err;
          }
        } else {
          if ((err = pn_bytes_format(bytes, ": "))) return err;
        }
      }
    }
    if ((err = pn_bytes_format(bytes, "%s",  list ? "]" : "}"))) return err;
    return 0;
  default:
    pn_format_datum(bytes, *datum);
    return 0;
  }
}

ssize_t pn_format_data(char *buf, size_t n, pn_data_t data)
{
  pn_bytes_t bytes = {n, buf};
  pn_data_t copy = data;
  while (copy.size)
  {
    int e = pn_format_data_one(&bytes, &copy, 0);
    if (e) return e;
  }

  return n - bytes.size;
}

int pn_pprint_data(const pn_data_t *data)
{
  int size = 4;

  while (true)
  {
    char buf[size];
    ssize_t n = pn_format_data(buf, size, *data);
    if (n < 0) {
      if (n == PN_OVERFLOW) {
        size *= 2;
        continue;
      } else {
        return n;
      }
    } else {
      printf("%.*s", (int) n, buf);
      return 0;
    }
  }
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

int pn_decode_one(pn_bytes_t *bytes, pn_data_t *data)
{
  pn_bytes_t buf = *bytes;
  pn_data_t dat = *data;

  int e = pn_decode_datum(&buf, &dat);
  if (e) return e;

  data->size -= dat.size;
  bytes->size -= buf.size;
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

int pn_fill_one(pn_data_t *data, const char *fmt, ...);

int pn_vfill_one(pn_data_t *data, const char **fmt, va_list ap, bool skip, int *nvals)
{
  int err, count;
  char code = **fmt;
  if (!code) return PN_ARG_ERR;
  if (!skip && !data->size) return PN_OVERFLOW;
  pn_datum_t skipped;
  pn_datum_t *datum;
  if (skip) {
    datum = &skipped;
  } else {
    datum = data->start;
    pn_data_ltrim(data, 1);
  }
  (*fmt)++;

  switch (code) {
  case 'n':
    *datum = (pn_datum_t) {PN_NULL, {0}};
    (*nvals)++;
    return 0;
  case 'o':
    datum->type = PN_BOOL;
    datum->u.as_bool = va_arg(ap, int);
    (*nvals)++;
    return 0;
  case 'B':
    datum->type = PN_UBYTE;
    datum->u.as_ubyte = va_arg(ap, unsigned int);
    (*nvals)++;
    return 0;
  case 'b':
    datum->type = PN_BYTE;
    datum->u.as_byte = va_arg(ap, int);
    (*nvals)++;
    return 0;
  case 'H':
    datum->type = PN_USHORT;
    datum->u.as_ushort = va_arg(ap, unsigned int);
    (*nvals)++;
    return 0;
  case 'h':
    datum->type = PN_SHORT;
    datum->u.as_short = va_arg(ap, int);
    (*nvals)++;
    return 0;
  case 'I':
    datum->type = PN_UINT;
    datum->u.as_uint = va_arg(ap, uint32_t);
    (*nvals)++;
    return 0;
  case 'i':
    datum->type = PN_INT;
    datum->u.as_int = va_arg(ap, int32_t);
    (*nvals)++;
    return 0;
  case 'L':
    datum->type = PN_ULONG;
    datum->u.as_ulong = va_arg(ap, uint64_t);
    (*nvals)++;
    return 0;
  case 'l':
    datum->type = PN_LONG;
    datum->u.as_long = va_arg(ap, int64_t);
    (*nvals)++;
    return 0;
  case 'f':
    datum->type = PN_FLOAT;
    datum->u.as_float = va_arg(ap, double);
    (*nvals)++;
    return 0;
  case 'd':
    datum->type = PN_DOUBLE;
    datum->u.as_double = va_arg(ap, double);
    (*nvals)++;
    return 0;
  case 'z':
    datum->type = PN_BINARY;
    datum->u.as_binary.size = va_arg(ap, size_t);
    datum->u.as_binary.start = va_arg(ap, char *);
    if (!datum->u.as_binary.start)
      *datum = (pn_datum_t) {PN_NULL, {0}};
    (*nvals)++;
    return 0;
  case 'S':
    datum->type = PN_STRING;
    datum->u.as_string.start = va_arg(ap, char *);
    if (datum->u.as_string.start)
      datum->u.as_string.size = strlen(datum->u.as_string.start);
    else
      *datum = (pn_datum_t) {PN_NULL, {0}};
    (*nvals)++;
    return 0;
  case 's':
    datum->type = PN_SYMBOL;
    datum->u.as_symbol.start = va_arg(ap, char *);
    if (datum->u.as_symbol.start)
      datum->u.as_symbol.size = strlen(datum->u.as_symbol.start);
    else
      *datum = (pn_datum_t) {PN_NULL, {0}};
    (*nvals)++;
    return 0;
  case 'D':
    *datum = (pn_datum_t) {PN_DESCRIPTOR, {0}};
    count = 0;
    err = pn_vfill_one(data, fmt, ap, skip, &count);
    if (err) return err;
    if (count != 1) return PN_ARG_ERR;
    err = pn_vfill_one(data, fmt, ap, skip, &count);
    if (err) return err;
    if (count != 2) return PN_ARG_ERR;
    (*nvals)++;
    return 0;
  case 'T':
    datum->type = PN_TYPE;
    datum->u.type = va_arg(ap, int);
    (*nvals)++;
    return 0;
  case '@':
    datum->type = PN_ARRAY;
    count = 0;
    err = pn_vfill_one(data, fmt, ap, skip, &count);
    if (err) return err;
    if (count != 1 || data->start[-1].type != PN_TYPE) {
      fprintf(stderr, "expected a single PN_TYPE, got %i data ending in %s\n",
              count, pn_type_str(data->start[-1].type));
      return PN_ARG_ERR;
    }
    if (**fmt != '[') {
      fprintf(stderr, "expected '['\n");
      return PN_ARG_ERR;
    }
    (*fmt)++;
    count = 0;
    while (**fmt && **fmt != ']') {
      err = pn_vfill_one(data, fmt, ap, skip, &count);
      if (err) return err;
    }
    if (**fmt != ']') {
      fprintf(stderr, "expected ']'\n");
      return PN_ARG_ERR;
    }
    (*fmt)++;
    datum->u.count = count;
    (*nvals)++;
    return 0;
  case '[':
    datum->type = PN_LIST;
    count = 0;
    while (**fmt && **fmt != ']') {
      err = pn_vfill_one(data, fmt, ap, skip, &count);
      if (err) return err;
    }
    if (**fmt != ']') {
      fprintf(stderr, "expected ']'\n");
      return PN_ARG_ERR;
    }
    (*fmt)++;
    datum->u.count = count;
    (*nvals)++;
    return 0;
  case '{':
    datum->type = PN_MAP;
    count = 0;
    while (**fmt && **fmt != '}') {
      err = pn_vfill_one(data, fmt, ap, skip, &count);
      if (err) return err;
    }
    if (**fmt != '}') {
      fprintf(stderr, "expected '}'\n");
      return PN_ARG_ERR;
    }
    (*fmt)++;
    datum->u.count = count;
    (*nvals)++;
    return 0;
  case '?':
    count = 0;
    if (va_arg(ap, int)) {
      // rewind data by one
      if (!skip) {
        data->start--;
        data->size++;
      }
      err = pn_vfill_one(data, fmt, ap, skip, &count);
      if (err) return err;
    } else {
      *datum = (pn_datum_t) {PN_NULL, {0}};
      err = pn_vfill_one(NULL, fmt, ap, true, &count);
      if (err) return err;
    }
    (*nvals)++;
    return 0;
  case '*':
    {
      int count = va_arg(ap, int);
      void *ptr = va_arg(ap, void *);
      // rewind data by one
      if (!skip) {
        data->start--;
        data->size++;
      }

      char c = **fmt;
      (*fmt)++;

      switch (c)
      {
      case 's':
        {
          char **sptr = ptr;
          for (int i = 0; i < count; i++)
          {
            char *sym = *(sptr++);
            err = pn_fill_one(data, "s", sym);
            if (err) return err;
            (*nvals)++;
          }
        }
        break;
      default:
        fprintf(stderr, "unrecognized * code: 0x%.2X '%c'\n", code, code);
        return PN_ARG_ERR;
      }

      return 0;
    }
  default:
    fprintf(stderr, "unrecognized fill code: 0x%.2X '%c'\n", code, code);
    return PN_ARG_ERR;
  }
}

int pn_fill_one(pn_data_t *data, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int count = 0;
  int err = pn_vfill_one(data, &fmt, ap, false, &count);
  va_end(ap);
  return err;
}

int pn_vfill_data(pn_data_t *data, const char *fmt, va_list ap)
{
  const char *pos = fmt;
  pn_data_t dbuf = *data;
  int count = 0;

  while (*pos) {
    int err = pn_vfill_one(&dbuf, &pos, ap, false, &count);
    if (err) return err;
  }

  data->size -= dbuf.size;
  return 0;
}

int pn_fill_data(pn_data_t *data, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int n = pn_vfill_data(data, fmt, ap);
  va_end(ap);
  return n;
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
    {
      if (data) pn_data_ltrim(data, 1);
      bool scd, scv;
      pn_data_t *rdata;
      if (datum && datum->type == PN_DESCRIPTOR) {
        rdata = data;
      } else {
        rdata = NULL;
      }
      if (!**fmt) {
        fprintf(stderr, "expecting descriptor\n");
        return PN_ARG_ERR;
      }
      err = pn_scan_one(rdata, fmt, ap, &scd);
      if (err) return err;
      if (!**fmt) {
        fprintf(stderr, "expecting described value\n");
        return PN_ARG_ERR;
      }
      err = pn_scan_one(rdata, fmt, ap, &scv);
      if (err) return err;
      *scanned = scd && scv;
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
      if (!**fmt) {
        fprintf(stderr, "type must follow array\n");
        return PN_ARG_ERR;
      }
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
    {
      if (data) pn_data_ltrim(data, 1);
      pn_data_t *rdata = datum && datum->type == PN_LIST ? data : NULL;
      count = 0;
      bool sc = true;
      while (**fmt && **fmt != ']') {
        bool sce;
        if (datum && count < datum->u.count) {
          err = pn_scan_one(rdata, fmt, ap, &sce);
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
      if (datum && count < datum->u.count) {
        err = pn_skip(rdata, datum->u.count - count);
        if (err) return err;
      }
      *scanned = (datum && datum->u.count == count) && sc;
    }
    return 0;
  case '{':
    if (data) pn_data_ltrim(data, 1);
    if (datum && datum->type == PN_MAP) {
      count = 0;
      bool sc = true;
      while (**fmt && **fmt != '}') {
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
    if (data) {
      err = pn_skip(data, 1);
      if (err) return err;
    }
    *scanned = true;
    return 0;
  case '?':
    {
      if (!**fmt) {
        fprintf(stderr, "codes must follow ?\n");
        return PN_ARG_ERR;
      }
      bool *sc = va_arg(ap, bool *);
      err = pn_scan_one(data, fmt, ap, sc);
      if (err) return err;
      *scanned = true;
    }
    return 0;
  default:
    fprintf(stderr, "unrecognized scan code: 0x%.2X '%c'\n", code, code);
    return PN_ARG_ERR;
  }
}

int pn_scan_data(const pn_data_t *data, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int err = pn_vscan_data(data, fmt, ap);
  va_end(ap);
  return err;
}

int pn_vscan_data(const pn_data_t *data, const char *fmt, va_list ap)
{
  pn_data_t dbuf = *data;
  const char *pos = fmt;
  bool scanned;

  while (*pos) {
    int err = pn_scan_one(&dbuf, &pos, ap, &scanned);
    if (err) return err;
  }

  return 0;
}

pn_bytes_t pn_bytes(size_t size, char *start)
{
  return (pn_bytes_t) {size, start};
}

pn_bytes_t pn_bytes_dup(size_t size, const char *start)
{
  if (size && start)
  {
    char *dup = malloc(size);
    memmove(dup, start, size);
    return pn_bytes(size, dup);
  } else {
    return pn_bytes(0, NULL);
  }
}
