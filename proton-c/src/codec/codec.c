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

#include <proton/codec.h>
#include <proton/errors.h>
#include <proton/buffer.h>
#include <proton/util.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <ctype.h>
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

size_t pn_bytes_ltrim(pn_bytes_t *bytes, size_t size);

int pn_bytes_format(pn_bytes_t *bytes, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(bytes->start, bytes->size, fmt, ap);
  va_end(ap);
  if (n >= bytes->size) {
    return PN_OVERFLOW;
  } else if (n >= 0) {
    pn_bytes_ltrim(bytes, n);
    return 0;
  } else {
    return PN_ERR;
  }
}

int pn_print_atom(pn_atom_t atom)
{
  size_t size = 4;
  while (true) {
    char buf[size];
    pn_bytes_t bytes = pn_bytes(size, buf);
    int err = pn_format_atom(&bytes, atom);
    if (err) {
      if (err == PN_OVERFLOW) {
        size *= 2;
        continue;
      } else {
        return err;
      }
    } else {
      printf("%.*s", (int) (size - bytes.size), buf);
      return 0;
    }
  }
}

int pn_format_atom(pn_bytes_t *bytes, pn_atom_t atom)
{
  switch (atom.type)
  {
  case PN_NULL:
    return pn_bytes_format(bytes, "null");
  case PN_BOOL:
    return pn_bytes_format(bytes, atom.u.as_bool ? "true" : "false");
  case PN_UBYTE:
    return pn_bytes_format(bytes, "%u", atom.u.as_ubyte);
  case PN_BYTE:
    return pn_bytes_format(bytes, "%i", atom.u.as_byte);
  case PN_USHORT:
    return pn_bytes_format(bytes, "%u", atom.u.as_ushort);
  case PN_SHORT:
    return pn_bytes_format(bytes, "%i", atom.u.as_short);
  case PN_UINT:
    return pn_bytes_format(bytes, "%u", atom.u.as_uint);
  case PN_INT:
    return pn_bytes_format(bytes, "%i", atom.u.as_int);
  case PN_ULONG:
    return pn_bytes_format(bytes, "%lu", atom.u.as_ulong);
  case PN_LONG:
    return pn_bytes_format(bytes, "%li", atom.u.as_long);
  case PN_FLOAT:
    return pn_bytes_format(bytes, "%g", atom.u.as_float);
  case PN_DOUBLE:
    return pn_bytes_format(bytes, "%g", atom.u.as_double);
  case PN_BINARY:
  case PN_STRING:
  case PN_SYMBOL:
    {
      int err;
      const char *pfx;
      pn_bytes_t bin;
      bool quote;
      switch (atom.type) {
      case PN_BINARY:
        pfx = "b";
        bin = atom.u.as_binary;
        quote = true;
        break;
      case PN_STRING:
        pfx = "";
        bin = atom.u.as_string;
        quote = true;
        break;
      case PN_SYMBOL:
        pfx = ":";
        bin = atom.u.as_symbol;
        quote = false;
        for (int i = 0; i < bin.size; i++) {
          if (!isalpha(bin.start[i])) {
            quote = true;
            break;
          }
        }
        break;
      default:
        pn_fatal("XXX");
        return PN_ERR;
      }

      if ((err = pn_bytes_format(bytes, "%s", pfx))) return err;
      if (quote) if ((err = pn_bytes_format(bytes, "\""))) return err;
      ssize_t n = pn_quote_data(bytes->start, bytes->size, bin.start, bin.size);
      if (n < 0) return n;
      pn_bytes_ltrim(bytes, n);
      if (quote) if ((err = pn_bytes_format(bytes, "\""))) return err;
      return 0;
    }
  case PN_DESCRIPTOR:
    return pn_bytes_format(bytes, "descriptor");
  case PN_ARRAY:
    return pn_bytes_format(bytes, "array[%zu]", atom.u.count);
  case PN_LIST:
    return pn_bytes_format(bytes, "list[%zu]", atom.u.count);
  case PN_MAP:
    return pn_bytes_format(bytes, "map[%zu]", atom.u.count);
  case PN_TYPE:
    return pn_bytes_format(bytes, "%s", pn_type_str(atom.u.type));
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

size_t pn_atoms_ltrim(pn_atoms_t *atoms, size_t size)
{
  if (size > atoms->size)
    size = atoms->size;

  atoms->start += size;
  atoms->size -= size;

  return size;
}

int pn_format_atoms_one(pn_bytes_t *bytes, pn_atoms_t *atoms, int level)
{
  if (!atoms->size) return PN_UNDERFLOW;
  pn_atom_t *atom = atoms->start;
  pn_atoms_ltrim(atoms, 1);
  int err, count;

  switch (atom->type) {
  case PN_DESCRIPTOR:
    if ((err = pn_bytes_format(bytes, "@"))) return err;
    if ((err = pn_format_atoms_one(bytes, atoms, level + 1))) return err;
    if ((err = pn_bytes_format(bytes, " "))) return err;
    if ((err = pn_format_atoms_one(bytes, atoms, level + 1))) return err;
    return 0;
  case PN_ARRAY:
    count = atom->u.count;
    if ((err = pn_bytes_format(bytes, "@"))) return err;
    if ((err = pn_format_atoms_one(bytes, atoms, level + 1))) return err;
    if ((err = pn_bytes_format(bytes, "["))) return err;
    for (int i = 0; i < count; i++)
    {
      if ((err = pn_format_atoms_one(bytes, atoms, level + 1))) return err;
      if (i < count - 1) {
        if ((err = pn_bytes_format(bytes, ", "))) return err;
      }
    }
    if ((err = pn_bytes_format(bytes, "]"))) return err;
    return 0;
  case PN_LIST:
  case PN_MAP:
    count = atom->u.count;
    bool list = atom->type == PN_LIST;
    if ((err = pn_bytes_format(bytes, "%s",  list ? "[" : "{"))) return err;
    for (int i = 0; i < count; i++)
    {
      if ((err = pn_format_atoms_one(bytes, atoms, level + 1))) return err;
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
          if ((err = pn_bytes_format(bytes, "="))) return err;
        }
      }
    }
    if ((err = pn_bytes_format(bytes, "%s",  list ? "]" : "}"))) return err;
    return 0;
  default:
    return pn_format_atom(bytes, *atom);
  }
}

ssize_t pn_format_atoms(char *buf, size_t n, pn_atoms_t atoms)
{
  pn_bytes_t bytes = {n, buf};
  pn_atoms_t copy = atoms;

  while (copy.size)
  {
    int e = pn_format_atoms_one(&bytes, &copy, 0);
    if (e) return e;
    if (copy.size) {
      e = pn_bytes_format(&bytes, " ");
      if (e) return e;
    }
  }

  return n - bytes.size;
}

int pn_print_atoms(const pn_atoms_t *atoms)
{
  int size = 4;

  while (true)
  {
    char buf[size];
    ssize_t n = pn_format_atoms(buf, size, *atoms);
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

int pn_decode_atom(pn_bytes_t *bytes, pn_atoms_t *atoms);
int pn_encode_atom(pn_bytes_t *bytes, pn_atoms_t *atoms);

int pn_decode_atoms(pn_bytes_t *bytes, pn_atoms_t *atoms)
{
  pn_bytes_t buf = *bytes;
  pn_atoms_t dat = *atoms;

  while (buf.size) {
    int e = pn_decode_atom(&buf, &dat);
    if (e) return e;
  }

  atoms->size -= dat.size;
  return 0;
}

int pn_decode_one(pn_bytes_t *bytes, pn_atoms_t *atoms)
{
  pn_bytes_t buf = *bytes;
  pn_atoms_t dat = *atoms;

  int e = pn_decode_atom(&buf, &dat);
  if (e) return e;

  atoms->size -= dat.size;
  bytes->size -= buf.size;
  return 0;
}

int pn_encode_atoms(pn_bytes_t *bytes, pn_atoms_t *atoms)
{
  pn_bytes_t buf = *bytes;
  pn_atoms_t dat = *atoms;

  while (dat.size) {
    int e = pn_encode_atom(&buf, &dat);
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

int pn_encode_type(pn_bytes_t *bytes, pn_atoms_t *atoms, pn_type_t *type);
int pn_encode_value(pn_bytes_t *bytes, pn_atoms_t *atoms, pn_type_t type);

int pn_encode_atom(pn_bytes_t *bytes, pn_atoms_t *atoms)
{
  pn_type_t type;
  int e = pn_encode_type(bytes, atoms, &type);
  if (e) return e;
  return pn_encode_value(bytes, atoms, type);
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

int pn_encode_type(pn_bytes_t *bytes, pn_atoms_t *atoms, pn_type_t *type)
{
  if (!atoms->size) return PN_UNDERFLOW;

  pn_atom_t *atom = atoms->start;
  if (atom->type == PN_DESCRIPTOR)
  {
    pn_atoms_ltrim(atoms, 1);
    int e = pn_bytes_writef8(bytes, 0);
    if (e) return e;
    e = pn_encode_atom(bytes, atoms);
    if (e) return e;
    return pn_encode_type(bytes, atoms, type);
  } else if (atom->type == PN_TYPE) {
    *type = atom->u.type;
  } else {
    *type = atom->type;
  }

  return pn_bytes_writef8(bytes, pn_type2code(*type));
}

int pn_encode_value(pn_bytes_t *bytes, pn_atoms_t *atoms, pn_type_t type)
{
  pn_atom_t *atom = atoms->start;
  int e;
  conv_t c;

  if (!atoms->size) return PN_UNDERFLOW;
  pn_atoms_ltrim(atoms, 1);

  switch (type)
  {
  case PN_NULL: return 0;
  case PN_BOOL: return pn_bytes_writef8(bytes, atom->u.as_bool);
  case PN_UBYTE: return pn_bytes_writef8(bytes, atom->u.as_ubyte);
  case PN_BYTE: return pn_bytes_writef8(bytes, atom->u.as_byte);
  case PN_USHORT: return pn_bytes_writef16(bytes, atom->u.as_ushort);
  case PN_SHORT: return pn_bytes_writef16(bytes, atom->u.as_short);
  case PN_UINT: return pn_bytes_writef32(bytes, atom->u.as_uint);
  case PN_INT: return pn_bytes_writef32(bytes, atom->u.as_int);
  case PN_ULONG: return pn_bytes_writef64(bytes, atom->u.as_ulong);
  case PN_LONG: return pn_bytes_writef64(bytes, atom->u.as_long);
  case PN_FLOAT: c.f = atom->u.as_float; return pn_bytes_writef32(bytes, c.i);
  case PN_DOUBLE: c.d = atom->u.as_double; return pn_bytes_writef32(bytes, c.l);
  case PN_BINARY: return pn_bytes_writev32(bytes, &atom->u.as_binary);
  case PN_STRING: return pn_bytes_writev32(bytes, &atom->u.as_string);
  case PN_SYMBOL: return pn_bytes_writev32(bytes, &atom->u.as_symbol);
  case PN_ARRAY:
    {
      char *start = bytes->start;
      // we'll backfill the size later
      if (bytes->size < 4) return PN_OVERFLOW;
      pn_bytes_ltrim(bytes, 4);
      size_t count = atom->u.count;
      e = pn_bytes_writef32(bytes, count);
      if (e) return e;
      pn_type_t atype;
      e = pn_encode_type(bytes, atoms, &atype);
      if (e) return e;
      // trim the type
      pn_atoms_ltrim(atoms, 1);

      for (int i = 0; i < count; i++)
      {
        e = pn_encode_value(bytes, atoms, atype);
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
      size_t count = atom->u.count;
      e = pn_bytes_writef32(bytes, count);
      if (e) return e;

      for (int i = 0; i < count; i++)
      {
        e = pn_encode_atom(bytes, atoms);
        if (e) return e;
      }

      // backfill size
      size_t size = bytes->start - start - 4;
      pn_bytes_t size_bytes = {4, start};
      pn_bytes_writef32(&size_bytes, size);
      return 0;
    }
  default:
    pn_fatal("atom has no value: %u", atom->u.type);
    return PN_ARG_ERR;
  }
}

int pn_decode_type(pn_bytes_t *bytes, pn_atoms_t *atoms, uint8_t *code)
{
  if (!bytes->size) return PN_UNDERFLOW;
  if (!atoms->size) return PN_OVERFLOW;

  if (bytes->start[0] != PNE_DESCRIPTOR) {
    *code = bytes->start[0];
    pn_bytes_ltrim(bytes, 1);
    return 0;
  } else {
    atoms->start[0] = (pn_atom_t) {.type=PN_DESCRIPTOR};
    pn_bytes_ltrim(bytes, 1);
    pn_atoms_ltrim(atoms, 1);
    int e = pn_decode_atom(bytes, atoms);
    if (e) return e;
    e = pn_decode_type(bytes, atoms, code);
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

int pn_decode_value(pn_bytes_t *bytes, pn_atoms_t *atoms, uint8_t code)
{
  size_t size;
  size_t count;
  conv_t conv;

  if (!atoms->size) return PN_OVERFLOW;

  pn_atom_t atom;

  switch (code)
  {
  case PNE_DESCRIPTOR:
    return PN_ARG_ERR;
  case PNE_NULL:
    atom.type=PN_NULL;
    break;
  case PNE_TRUE:
    atom.type=PN_BOOL, atom.u.as_bool=true;
    break;
  case PNE_FALSE:
    atom.type=PN_BOOL, atom.u.as_bool=false;
    break;
  case PNE_BOOLEAN:
    if (!bytes->size) return PN_UNDERFLOW;
    atom.type=PN_BOOL, atom.u.as_bool=(*(bytes->start) != 0);
    pn_bytes_ltrim(bytes, 1);
    break;
  case PNE_UBYTE:
    if (!bytes->size) return PN_UNDERFLOW;
    atom.type=PN_UBYTE, atom.u.as_ubyte=*((uint8_t *) (bytes->start));
    pn_bytes_ltrim(bytes, 1);
    break;
  case PNE_BYTE:
    if (!bytes->size) return PN_UNDERFLOW;
    atom.type=PN_BYTE, atom.u.as_byte=*((int8_t *) (bytes->start));
    pn_bytes_ltrim(bytes, 1);
    break;
  case PNE_USHORT:
    if (bytes->size < 2) return PN_UNDERFLOW;
    atom.type=PN_USHORT, atom.u.as_ushort=ntohs(*((uint16_t *) (bytes->start)));
    pn_bytes_ltrim(bytes, 2);
    break;
  case PNE_SHORT:
    if (bytes->size < 2) return PN_UNDERFLOW;
    atom.type=PN_SHORT, atom.u.as_short=ntohs(*((int16_t *) (bytes->start)));
    pn_bytes_ltrim(bytes, 2);
    break;
  case PNE_UINT:
    if (bytes->size < 4) return PN_UNDERFLOW;
    atom.type=PN_UINT, atom.u.as_uint=ntohl(*((uint32_t *) (bytes->start)));
    pn_bytes_ltrim(bytes, 4);
    break;
  case PNE_UINT0:
    atom.type=PN_UINT, atom.u.as_uint=0;
    break;
  case PNE_SMALLUINT:
    if (!bytes->size) return PN_UNDERFLOW;
    atom.type=PN_UINT, atom.u.as_uint=*((uint8_t *) (bytes->start));
    pn_bytes_ltrim(bytes, 1);
    break;
  case PNE_INT:
    if (bytes->size < 4) return PN_UNDERFLOW;
    atom.type=PN_INT, atom.u.as_int=ntohl(*((uint32_t *) (bytes->start)));
    pn_bytes_ltrim(bytes, 4);
    break;
  case PNE_FLOAT:
    if (bytes->size < 4) return PN_UNDERFLOW;
    // XXX: this assumes the platform uses IEEE floats
    conv.i = ntohl(*((uint32_t *) (bytes->start)));
    atom.type=PN_FLOAT, atom.u.as_float=conv.f;
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
      atom.type=PN_ULONG, atom.u.as_ulong=conv.l;
      break;
    case PNE_LONG:
      atom.type=PN_LONG, atom.u.as_long=(int64_t) conv.l;
      break;
    case PNE_DOUBLE:
      // XXX: this assumes the platform uses IEEE floats
      atom.type=PN_DOUBLE, atom.u.as_double=conv.d;
      break;
    default:
      return PN_ARG_ERR;
    }

    pn_bytes_ltrim(bytes, 8);
    break;
  case PNE_ULONG0:
    atom.type=PN_ULONG, atom.u.as_ulong=0;
    break;
  case PNE_SMALLULONG:
    if (!bytes->size) return PN_UNDERFLOW;
    atom.type=PN_ULONG, atom.u.as_ulong=*((uint8_t *) (bytes->start));
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
        atom.type=PN_BINARY, atom.u.as_binary=binary;
        break;
      case 0x1:
        atom.type=PN_STRING, atom.u.as_binary=binary;
        break;
      case 0x3:
        atom.type=PN_SYMBOL, atom.u.as_binary=binary;
        break;
      default:
        return PN_ARG_ERR;
      }
    }

    if (bytes->size < size) return PN_UNDERFLOW;
    pn_bytes_ltrim(bytes, size);
    break;
  case PNE_LIST0:
    atom.type=PN_LIST, atom.u.count=0;
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
        if (!atoms->size) return PN_OVERFLOW;
        atoms->start[0] = (pn_atom_t) {.type=PN_ARRAY, .u.count=count};
        pn_atoms_ltrim(atoms, 1);
        uint8_t acode;
        int e = pn_decode_type(bytes, atoms, &acode);
        if (e) return e;
        if (!atoms->size) return PN_OVERFLOW;
        pn_type_t type = pn_code2type(acode);
        if (type < 0) return type;
        atoms->start[0] = (pn_atom_t) {.type=PN_TYPE, .u.type=type};
        pn_atoms_ltrim(atoms, 1);
        for (int i = 0; i < count; i++)
        {
          e = pn_decode_value(bytes, atoms, acode);
          if (e) return e;
        }
      }
      return 0;
    case PNE_LIST8:
    case PNE_LIST32:
      if (!atoms->size) return PN_OVERFLOW;
      atoms->start[0] = (pn_atom_t) {.type=PN_LIST, .u.count=count};
      pn_atoms_ltrim(atoms, 1);
      break;
    case PNE_MAP8:
    case PNE_MAP32:
      if (!atoms->size) return PN_OVERFLOW;
      atoms->start[0] = (pn_atom_t) {.type=PN_MAP, .u.count=count};
      pn_atoms_ltrim(atoms, 1);
      break;
    default:
      return PN_ARG_ERR;
    }

    for (int i = 0; i < count; i++)
    {
      int e = pn_decode_atom(bytes, atoms);
      if (e) return e;
    }

    return 0;
  default:
    printf("Unrecognised typecode: %u\n", code);
    return PN_ARG_ERR;
  }

  if (!atoms->size) return PN_OVERFLOW;
  atoms->start[0] = atom;
  pn_atoms_ltrim(atoms, 1);

  return 0;
}

int pn_decode_atom(pn_bytes_t *bytes, pn_atoms_t *atoms)
{
  uint8_t code;
  int e;

  if ((e = pn_decode_type(bytes, atoms, &code))) return e;
  if ((e = pn_decode_value(bytes, atoms, code))) return e;

  return 0;
}

int pn_fill_one(pn_atoms_t *atoms, const char *fmt, ...);

int pn_vfill_one(pn_atoms_t *atoms, const char **fmt, va_list ap, bool skip, int *nvals)
{
  int err, count;
  char code = **fmt;
  if (!code) return PN_ARG_ERR;
  if (!skip && !atoms->size) return PN_OVERFLOW;
  pn_atom_t skipped;
  pn_atom_t *atom;
  if (skip) {
    atom = &skipped;
  } else {
    atom = atoms->start;
    pn_atoms_ltrim(atoms, 1);
  }
  (*fmt)++;

  switch (code) {
  case 'n':
    *atom = (pn_atom_t) {PN_NULL, {0}};
    (*nvals)++;
    return 0;
  case 'o':
    atom->type = PN_BOOL;
    atom->u.as_bool = va_arg(ap, int);
    (*nvals)++;
    return 0;
  case 'B':
    atom->type = PN_UBYTE;
    atom->u.as_ubyte = va_arg(ap, unsigned int);
    (*nvals)++;
    return 0;
  case 'b':
    atom->type = PN_BYTE;
    atom->u.as_byte = va_arg(ap, int);
    (*nvals)++;
    return 0;
  case 'H':
    atom->type = PN_USHORT;
    atom->u.as_ushort = va_arg(ap, unsigned int);
    (*nvals)++;
    return 0;
  case 'h':
    atom->type = PN_SHORT;
    atom->u.as_short = va_arg(ap, int);
    (*nvals)++;
    return 0;
  case 'I':
    atom->type = PN_UINT;
    atom->u.as_uint = va_arg(ap, uint32_t);
    (*nvals)++;
    return 0;
  case 'i':
    atom->type = PN_INT;
    atom->u.as_int = va_arg(ap, int32_t);
    (*nvals)++;
    return 0;
  case 'L':
    atom->type = PN_ULONG;
    atom->u.as_ulong = va_arg(ap, uint64_t);
    (*nvals)++;
    return 0;
  case 'l':
    atom->type = PN_LONG;
    atom->u.as_long = va_arg(ap, int64_t);
    (*nvals)++;
    return 0;
  case 'f':
    atom->type = PN_FLOAT;
    atom->u.as_float = va_arg(ap, double);
    (*nvals)++;
    return 0;
  case 'd':
    atom->type = PN_DOUBLE;
    atom->u.as_double = va_arg(ap, double);
    (*nvals)++;
    return 0;
  case 'z':
    atom->type = PN_BINARY;
    atom->u.as_binary.size = va_arg(ap, size_t);
    atom->u.as_binary.start = va_arg(ap, char *);
    if (!atom->u.as_binary.start)
      *atom = (pn_atom_t) {PN_NULL, {0}};
    (*nvals)++;
    return 0;
  case 'S':
    atom->type = PN_STRING;
    atom->u.as_string.start = va_arg(ap, char *);
    if (atom->u.as_string.start)
      atom->u.as_string.size = strlen(atom->u.as_string.start);
    else
      *atom = (pn_atom_t) {PN_NULL, {0}};
    (*nvals)++;
    return 0;
  case 's':
    atom->type = PN_SYMBOL;
    atom->u.as_symbol.start = va_arg(ap, char *);
    if (atom->u.as_symbol.start)
      atom->u.as_symbol.size = strlen(atom->u.as_symbol.start);
    else
      *atom = (pn_atom_t) {PN_NULL, {0}};
    (*nvals)++;
    return 0;
  case 'D':
    *atom = (pn_atom_t) {PN_DESCRIPTOR, {0}};
    count = 0;
    err = pn_vfill_one(atoms, fmt, ap, skip, &count);
    if (err) return err;
    if (count != 1) return PN_ARG_ERR;
    err = pn_vfill_one(atoms, fmt, ap, skip, &count);
    if (err) return err;
    if (count != 2) return PN_ARG_ERR;
    (*nvals)++;
    return 0;
  case 'T':
    atom->type = PN_TYPE;
    atom->u.type = va_arg(ap, int);
    (*nvals)++;
    return 0;
  case '@':
    atom->type = PN_ARRAY;
    count = 0;
    err = pn_vfill_one(atoms, fmt, ap, skip, &count);
    if (err) return err;
    if (count != 1 || atoms->start[-1].type != PN_TYPE) {
      fprintf(stderr, "expected a single PN_TYPE, got %i atoms ending in %s\n",
              count, pn_type_str(atoms->start[-1].type));
      return PN_ARG_ERR;
    }
    if (**fmt != '[') {
      fprintf(stderr, "expected '['\n");
      return PN_ARG_ERR;
    }
    (*fmt)++;
    count = 0;
    while (**fmt && **fmt != ']') {
      err = pn_vfill_one(atoms, fmt, ap, skip, &count);
      if (err) return err;
    }
    if (**fmt != ']') {
      fprintf(stderr, "expected ']'\n");
      return PN_ARG_ERR;
    }
    (*fmt)++;
    atom->u.count = count;
    (*nvals)++;
    return 0;
  case '[':
    atom->type = PN_LIST;
    count = 0;
    while (**fmt && **fmt != ']') {
      err = pn_vfill_one(atoms, fmt, ap, skip, &count);
      if (err) return err;
    }
    if (**fmt != ']') {
      fprintf(stderr, "expected ']'\n");
      return PN_ARG_ERR;
    }
    (*fmt)++;
    atom->u.count = count;
    (*nvals)++;
    return 0;
  case '{':
    atom->type = PN_MAP;
    count = 0;
    while (**fmt && **fmt != '}') {
      err = pn_vfill_one(atoms, fmt, ap, skip, &count);
      if (err) return err;
    }
    if (**fmt != '}') {
      fprintf(stderr, "expected '}'\n");
      return PN_ARG_ERR;
    }
    (*fmt)++;
    atom->u.count = count;
    (*nvals)++;
    return 0;
  case '?':
    count = 0;
    if (va_arg(ap, int)) {
      // rewind atoms by one
      if (!skip) {
        atoms->start--;
        atoms->size++;
      }
      err = pn_vfill_one(atoms, fmt, ap, skip, &count);
      if (err) return err;
    } else {
      *atom = (pn_atom_t) {PN_NULL, {0}};
      err = pn_vfill_one(NULL, fmt, ap, true, &count);
      if (err) return err;
    }
    (*nvals)++;
    return 0;
  case '*':
    {
      int count = va_arg(ap, int);
      void *ptr = va_arg(ap, void *);
      // rewind atoms by one
      if (!skip) {
        atoms->start--;
        atoms->size++;
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
            err = pn_fill_one(atoms, "s", sym);
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

int pn_fill_one(pn_atoms_t *atoms, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int count = 0;
  int err = pn_vfill_one(atoms, &fmt, ap, false, &count);
  va_end(ap);
  return err;
}

int pn_vfill_atoms(pn_atoms_t *atoms, const char *fmt, va_list ap)
{
  const char *pos = fmt;
  pn_atoms_t copy = *atoms;
  int count = 0;

  while (*pos) {
    int err = pn_vfill_one(&copy, &pos, ap, false, &count);
    if (err) return err;
  }

  atoms->size -= copy.size;
  return 0;
}

int pn_fill_atoms(pn_atoms_t *atoms, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int n = pn_vfill_atoms(atoms, fmt, ap);
  va_end(ap);
  return n;
}

int pn_vifill_atoms(pn_atoms_t *atoms, const char *fmt, va_list ap)
{
  pn_atoms_t copy = *atoms;
  int err = pn_vfill_atoms(&copy, fmt, ap);
  if (err) return err;
  atoms->start = copy.start + copy.size;
  atoms->size -= copy.size;
  return 0;
}

int pn_ifill_atoms(pn_atoms_t *atoms, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int err = pn_vifill_atoms(atoms, fmt, ap);
  va_end(ap);
  return err;
}

int pn_skip(pn_atoms_t *atoms, size_t n)
{
  for (int i = 0; i < n; i++) {
    if (!atoms->size) return PN_UNDERFLOW;
    pn_atom_t *atom = atoms->start;
    pn_atoms_ltrim(atoms, 1);
    int err;

    switch (atom->type)
    {
    case PN_DESCRIPTOR:
      err = pn_skip(atoms, 2);
      if (err) return err;
      break;
    case PN_ARRAY:
      err = pn_skip(atoms, 1);
      if (err) return err;
      err = pn_skip(atoms, atom->u.count);
      if (err) return err;
      break;
    case PN_LIST:
    case PN_MAP:
      err = pn_skip(atoms, atom->u.count);
      if (err) return err;
      break;
    default:
      break;
    }
  }

  return 0;
}

int pn_scan_one(pn_atoms_t *atoms, const char **fmt, va_list ap, bool *scanned)
{
  char code = **fmt;
  pn_atom_t *atom = atoms ? atoms->start : NULL;
  (*fmt)++;
  size_t count;
  int err;

  switch (code) {
  case 'n':
    if (atom && atom->type == PN_NULL) {
      *scanned = true;
    } else {
      *scanned = false;
    }
    if (atoms) pn_atoms_ltrim(atoms, 1);
    return 0;
  case 'o':
    {
      bool *value = va_arg(ap, bool *);
      if (atom && atom->type == PN_BOOL) {
        *value = atom->u.as_bool;
        *scanned = true;
      } else {
        *value = 0;
        *scanned = false;
      }
    }
    if (atoms) pn_atoms_ltrim(atoms, 1);
    return 0;
  case 'B':
    {
      uint8_t *value = va_arg(ap, uint8_t *);
      if (atom && atom->type == PN_UBYTE) {
        *value = atom->u.as_ubyte;
        *scanned = true;
      } else {
        *value = 0;
        *scanned = false;
      }
    }
    if (atoms) pn_atoms_ltrim(atoms, 1);
    return 0;
  case 'b':
    {
      int8_t *value = va_arg(ap, int8_t *);
      if (atom && atom->type == PN_BYTE) {
        *value = atom->u.as_byte;
        *scanned = true;
      } else {
        *value = 0;
        *scanned = false;
      }
    }
    if (atoms) pn_atoms_ltrim(atoms, 1);
    return 0;
  case 'H':
    {
      uint16_t *value = va_arg(ap, uint16_t *);
      if (atom && atom->type == PN_USHORT) {
        *value = atom->u.as_ushort;
        *scanned = true;
      } else {
        *value = 0;
        *scanned = false;
      }
    }
    if (atoms) pn_atoms_ltrim(atoms, 1);
    return 0;
  case 'h':
    {
      int16_t *value = va_arg(ap, int16_t *);
      if (atom && atom->type == PN_SHORT) {
        *value = atom->u.as_short;
        *scanned = true;
      } else {
        *value = 0;
        *scanned = false;
      }
    }
    if (atoms) pn_atoms_ltrim(atoms, 1);
    return 0;
  case 'I':
    {
      uint32_t *value = va_arg(ap, uint32_t *);
      if (atom && atom->type == PN_UINT) {
        *value = atom->u.as_uint;
        *scanned = true;
      } else {
        *value = 0;
        *scanned = false;
      }
    }
    if (atoms) pn_atoms_ltrim(atoms, 1);
    return 0;
  case 'i':
    {
      int32_t *value = va_arg(ap, int32_t *);
      if (atom && atom->type == PN_INT) {
        *value = atom->u.as_int;
        *scanned = true;
      } else {
        *value = 0;
        *scanned = false;
      }
    }
    if (atoms) pn_atoms_ltrim(atoms, 1);
    return 0;
  case 'L':
    {
      uint64_t *value = va_arg(ap, uint64_t *);
      if (atom && atom->type == PN_ULONG) {
        *value = atom->u.as_ulong;
        *scanned = true;
      } else {
        *value = 0;
        *scanned = false;
      }
    }
    if (atoms) pn_atoms_ltrim(atoms, 1);
    return 0;
  case 'l':
    {
      int64_t *value = va_arg(ap, int64_t *);
      if (atom && atom->type == PN_LONG) {
        *value = atom->u.as_long;
        *scanned = true;
      } else {
        *value = 0;
        *scanned = false;
      }
    }
    if (atoms) pn_atoms_ltrim(atoms, 1);
    return 0;
  case 'f':
    {
      float *value = va_arg(ap, float *);
      if (atom && atom->type == PN_FLOAT) {
        *value = atom->u.as_float;
        *scanned = true;
      } else {
        *value = 0;
        *scanned = false;
      }
    }
    if (atoms) pn_atoms_ltrim(atoms, 1);
    return 0;
  case 'd':
    {
      double *value = va_arg(ap, double *);
      if (atom && atom->type == PN_DOUBLE) {
        *value = atom->u.as_double;
        *scanned = true;
      } else {
        *value = 0;
        *scanned = false;
      }
    }
    if (atoms) pn_atoms_ltrim(atoms, 1);
    return 0;
  case 'z':
    {
      pn_bytes_t *bytes = va_arg(ap, pn_bytes_t *);
      if (atom && atom->type == PN_BINARY) {
        *bytes = atom->u.as_binary;
        *scanned = true;
      } else {
        *bytes = (pn_bytes_t) {0, 0};
        *scanned = false;
      }
    }
    if (atoms) pn_atoms_ltrim(atoms, 1);
    return 0;
  case 'S':
    {
      pn_bytes_t *bytes = va_arg(ap, pn_bytes_t *);
      if (atom && atom->type == PN_STRING) {
        *bytes = atom->u.as_string;
        *scanned = true;
      } else {
        *bytes = (pn_bytes_t) {0, 0};
        *scanned = false;
      }
    }
    if (atoms) pn_atoms_ltrim(atoms, 1);
    return 0;
  case 's':
    {
      pn_bytes_t *bytes = va_arg(ap, pn_bytes_t *);
      if (atom && atom->type == PN_SYMBOL) {
        *bytes = atom->u.as_symbol;
        *scanned = true;
      } else {
        *bytes = (pn_bytes_t) {0, 0};
        *scanned = false;
      }
    }
    if (atoms) pn_atoms_ltrim(atoms, 1);
    return 0;
  case 'D':
    {
      if (atoms) pn_atoms_ltrim(atoms, 1);
      bool scd, scv;
      pn_atoms_t *ratoms;
      if (atom && atom->type == PN_DESCRIPTOR) {
        ratoms = atoms;
      } else {
        ratoms = NULL;
      }
      if (!**fmt) {
        fprintf(stderr, "expecting descriptor\n");
        return PN_ARG_ERR;
      }
      err = pn_scan_one(ratoms, fmt, ap, &scd);
      if (err) return err;
      if (!**fmt) {
        fprintf(stderr, "expecting described value\n");
        return PN_ARG_ERR;
      }
      err = pn_scan_one(ratoms, fmt, ap, &scv);
      if (err) return err;
      *scanned = scd && scv;
    }
    return 0;
  case 'T':
    if (atom && atom->type == PN_TYPE) {
      pn_type_t *type = va_arg(ap, pn_type_t *);
      *type = atom->u.type;
      *scanned = true;
    } else {
      *scanned = false;
    }
    if (atoms) pn_atoms_ltrim(atoms, 1);
    return 0;
  case '@':
    if (atoms) pn_atoms_ltrim(atoms, 1);
    if (atom && atom->type == PN_ARRAY) {
      bool sc;
      if (!**fmt) {
        fprintf(stderr, "type must follow array\n");
        return PN_ARG_ERR;
      }
      err = pn_scan_one(atoms, fmt, ap, &sc);
      if (err) return err;
      if (**fmt != '[') {
        fprintf(stderr, "expected '['\n");
        return PN_ARG_ERR;
      }
      (*fmt)++;
      count = 0;
      while (**fmt && **fmt != ']') {
        bool sce;
        if (count < atom->u.count) {
          err = pn_scan_one(atoms, fmt, ap, &sce);
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
      if (count < atom->u.count) {
        err = pn_skip(atoms, atom->u.count - count);
        if (err) return err;
      }
      *scanned = (atom->u.count == count) && sc;
    } else {
      *scanned = false;
    }
    return 0;
  case '[':
    {
      if (atoms) pn_atoms_ltrim(atoms, 1);
      pn_atoms_t *ratoms = atom && atom->type == PN_LIST ? atoms : NULL;
      count = 0;
      bool sc = true;
      while (**fmt && **fmt != ']') {
        bool sce;
        if (atom && count < atom->u.count) {
          err = pn_scan_one(ratoms, fmt, ap, &sce);
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
      if (atom && count < atom->u.count) {
        err = pn_skip(ratoms, atom->u.count - count);
        if (err) return err;
      }
      *scanned = (atom && atom->u.count == count) && sc;
    }
    return 0;
  case '{':
    if (atoms) pn_atoms_ltrim(atoms, 1);
    if (atom && atom->type == PN_MAP) {
      count = 0;
      bool sc = true;
      while (**fmt && **fmt != '}') {
        bool sce;
        if (count < atom->u.count) {
          err = pn_scan_one(atoms, fmt, ap, &sce);
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
      if (count < atom->u.count) {
        err = pn_skip(atoms, atom->u.count - count);
        if (err) return err;
      }
      *scanned = (atom->u.count == count) && sc;
    } else {
      *scanned = false;
    }
    return 0;
  case '.':
    if (atoms) {
      err = pn_skip(atoms, 1);
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
      err = pn_scan_one(atoms, fmt, ap, sc);
      if (err) return err;
      *scanned = true;
    }
    return 0;
  default:
    fprintf(stderr, "unrecognized scan code: 0x%.2X '%c'\n", code, code);
    return PN_ARG_ERR;
  }
}

int pn_scan_atoms(const pn_atoms_t *atoms, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int err = pn_vscan_atoms(atoms, fmt, ap);
  va_end(ap);
  return err;
}

int pn_vscan_atoms(const pn_atoms_t *atoms, const char *fmt, va_list ap)
{
  pn_atoms_t copy = *atoms;
  const char *pos = fmt;
  bool scanned;

  while (*pos) {
    int err = pn_scan_one(&copy, &pos, ap, &scanned);
    if (err) return err;
  }

  return 0;
}

// data

struct pn_data_t {
  size_t capacity;
  size_t size;
  pn_atom_t *atoms;
  bool deep;
  pn_buffer_t *buf;
};


pn_data_t *pn_data(size_t capacity)
{
  pn_data_t *data = malloc(sizeof(pn_data_t));
  data->capacity = capacity;
  data->size = 0;
  data->atoms = capacity ? malloc(capacity * sizeof(pn_atom_t)) : NULL;
  data->deep = false;
  data->buf = NULL;
  return data;
}

void pn_data_free(pn_data_t *data)
{
  if (data) {
    pn_buffer_free(data->buf);
    free(data->atoms);
    free(data);
  }
}

int pn_data_clear(pn_data_t *data)
{
  if (data) {
    data->size = 0;
    if (data->buf) {
      int e = pn_buffer_clear(data->buf);
      if (e) return e;
    }
  }
  return 0;
}

int pn_data_grow(pn_data_t *data)
{
  data->capacity = 2*(data->capacity ? data->capacity : 16);
  data->atoms = realloc(data->atoms, data->capacity * sizeof(pn_atom_t));
  return 0;
}

int pn_data_decode(pn_data_t *data, char *bytes, size_t *size)
{
  pn_atoms_t atoms;
  pn_bytes_t lbytes;

  while (true) {
    atoms.size = data->capacity;
    atoms.start = data->atoms;
    lbytes.size = *size;
    lbytes.start = bytes;

    int err = pn_decode_one(&lbytes, &atoms);

    if (!err) {
      data->size = atoms.size;
      *size = lbytes.size;
      return 0;
    } else if (err == PN_OVERFLOW) {
      err = pn_data_grow(data);
      if (err) return err;
      atoms.size = data->capacity;
    } else {
      return err;
    }
  }
}

int pn_data_encode(pn_data_t *data, char *bytes, size_t *size)
{
  pn_atoms_t atoms = {.size=data->size, .start=data->atoms};
  pn_bytes_t lbytes = pn_bytes(*size, bytes);

  int err = pn_encode_atoms(&lbytes, &atoms);
  if (err) return err;

  *size = lbytes.size;
  return 0;
}

int pn_data_vfill(pn_data_t *data, const char *fmt, va_list ap)
{
  pn_atoms_t atoms;

  while (true) {
    va_list cp;
    va_copy(cp, ap);
    atoms.size=data->capacity - data->size;
    atoms.start=data->atoms + data->size;
    int err = pn_vfill_atoms(&atoms, fmt, cp);
    if (!err) {
      data->size += atoms.size;
      return 0;
    } else if (err == PN_OVERFLOW) {
      err = pn_data_grow(data);
      if (err) return err;
    } else {
      return err;
    }
  }
}

int pn_data_fill(pn_data_t *data, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int err = pn_data_vfill(data, fmt, ap);
  va_end(ap);
  return err;
}

int pn_data_vscan(pn_data_t *data, const char *fmt, va_list ap)
{
  pn_atoms_t atoms = {.size=data->size, .start=data->atoms};
  return pn_vscan_atoms(&atoms, fmt, ap);
}

int pn_data_scan(pn_data_t *data, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int err = pn_data_vscan(data, fmt, ap);
  va_end(ap);
  return err;
}

int pn_data_print(pn_data_t *data)
{
  pn_atoms_t atoms = {.size=data->size, .start=data->atoms};
  return pn_print_atoms(&atoms);
}

pn_atoms_t pn_data_atoms(pn_data_t *data)
{
  return (pn_atoms_t) {.size=data->size, .start=data->atoms};
}

pn_atoms_t pn_data_available(pn_data_t *data)
{
  return (pn_atoms_t) {.size=data->capacity - data->size,
      .start=data->atoms + data->size};
}

int pn_data_format(pn_data_t *data, char *bytes, size_t *size)
{
  ssize_t sz = pn_format_atoms(bytes, *size, pn_data_atoms(data));
  if (sz < 0) {
    return sz;
  } else {
    *size = sz;
    return 0;
  }
}

int pn_data_resize(pn_data_t *data, size_t size)
{
  if (!data || size > data->capacity) return PN_ARG_ERR;
  data->size = size;
  return 0;
}
