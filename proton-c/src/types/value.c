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

#include <proton/codec.h>
#include <proton/util.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "encodings.h"
#include "value-internal.h"

int pn_compare_value(pn_value_t a, pn_value_t b)
{
  if (a.type == b.type) {
    switch (a.type)
    {
    case EMPTY:
      return 0;
    case BOOLEAN:
      return b.u.as_boolean && a.u.as_boolean;
    case UBYTE:
      return b.u.as_ubyte - a.u.as_ubyte;
    case USHORT:
      return b.u.as_ushort - a.u.as_ushort;
    case UINT:
      return b.u.as_uint - a.u.as_uint;
    case ULONG:
      return b.u.as_ulong - a.u.as_ulong;
    case BYTE:
      return b.u.as_byte - a.u.as_byte;
    case SHORT:
      return b.u.as_short - a.u.as_short;
    case INT:
      return b.u.as_int - a.u.as_int;
    case LONG:
      return b.u.as_long - a.u.as_long;
    case FLOAT:
      return b.u.as_float - a.u.as_float;
    case DOUBLE:
      return b.u.as_double - a.u.as_double;
    case CHAR:
      return b.u.as_char - a.u.as_char;
    case SYMBOL:
      return pn_compare_symbol(a.u.as_symbol, b.u.as_symbol);
    case STRING:
      return pn_compare_string(a.u.as_string, b.u.as_string);
    case BINARY:
      return pn_compare_binary(a.u.as_binary, b.u.as_binary);
    case REF:
      return (char *)b.u.as_ref - (char *)a.u.as_ref;
    default:
      pn_fatal("uncomparable: %s, %s", pn_aformat(a), pn_aformat(b));
      return -1;
    }
  } else {
    return b.type - a.type;
  }
}

uintptr_t pn_hash_value(pn_value_t v)
{
  switch (v.type)
  {
  case EMPTY:
    return 0;
  case UBYTE:
    return v.u.as_ubyte;
  case USHORT:
    return v.u.as_ushort;
  case UINT:
    return v.u.as_uint;
  case ULONG:
    return v.u.as_ulong;
  case BYTE:
    return v.u.as_byte;
  case SHORT:
    return v.u.as_short;
  case INT:
    return v.u.as_int;
  case LONG:
    return v.u.as_long;
  case FLOAT:
    return v.u.as_float;
  case DOUBLE:
    return v.u.as_double;
  case CHAR:
    return v.u.as_char;
  case SYMBOL:
    // XXX
    return 0;
  case STRING:
    return pn_hash_string(v.u.as_string);
  case BINARY:
    return pn_hash_binary(v.u.as_binary);
  default:
    return 0;
  }
}

int scan_size(const char *str)
{
  int i, level = 0, size = 0;
  for (i = 0; str[i]; i++)
  {
    switch (str[i])
    {
    case '@':
      i++;
      break;
    case '(':
    case '[':
    case '{':
      level++;
      break;
    case ')':
    case ']':
    case '}':
      if (level == 0)
        return size;
      level--;
      // fall through intentionally here
    default:
      if (level == 0) size++;
      break;
    }
  }

  return -1;
}

int pn_scan(pn_value_t *value, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int n = pn_vscan(value, fmt, ap);
  va_end(ap);
  return n;
}

static enum TYPE code_to_type(char c)
{
  switch (c)
  {
  case 'n': return EMPTY;
  case 'b': return BYTE;
  case 'B': return UBYTE;
  case 'h': return SHORT;
  case 'H': return USHORT;
  case 'i': return INT;
  case 'I': return UINT;
  case 'l': return LONG;
  case 'L': return ULONG;
  case 'f': return FLOAT;
  case 'd': return DOUBLE;
  case 'C': return CHAR;
  case 's': return SYMBOL;
  case 'S': return STRING;
  case 'z': return BINARY;
  case 't': return LIST;
  case 'm': return MAP;
  default:
    pn_fatal("unrecognized code: %c", c);
    return -1;
  }
}

typedef struct stack_frame_st {
  pn_value_t *value;
  int count;
} stack_frame_t;

int pn_vscan(pn_value_t *value, const char *fmt, va_list ap)
{
  stack_frame_t stack[strlen(fmt)];
  int level = 0, count = 0;

  for ( ; *fmt; fmt++)
  {
    if (level == 0)
      count++;

    switch (*fmt)
    {
    case 'n':
      value->type = EMPTY;
      break;
    case 'b':
      value->type = BYTE;
      value->u.as_byte = va_arg(ap, int);
      break;
    case 'B':
      value->type = UBYTE;
      value->u.as_ubyte = va_arg(ap, unsigned int);
      break;
    case 'h':
      value->type = SHORT;
      value->u.as_short = va_arg(ap, int);
      break;
    case 'H':
      value->type = USHORT;
      value->u.as_ushort = va_arg(ap, unsigned int);
      break;
    case 'i':
      value->type = INT;
      value->u.as_int = va_arg(ap, int32_t);
      break;
    case 'I':
      value->type = UINT;
      value->u.as_uint = va_arg(ap, uint32_t);
      break;
    case 'l':
      value->type = LONG;
      value->u.as_long = va_arg(ap, int64_t);
      break;
    case 'L':
      value->type = ULONG;
      value->u.as_ulong = va_arg(ap, uint64_t);
      break;
    case 'f':
      value->type = FLOAT;
      value->u.as_float = va_arg(ap, double);
      break;
    case 'd':
      value->type = DOUBLE;
      value->u.as_double = va_arg(ap, double);
      break;
    case 'C':
      value->type = CHAR;
      value->u.as_char = va_arg(ap, wchar_t);
      break;
    case 's':
      value->type = SYMBOL;
      value->u.as_symbol = pn_symbol(va_arg(ap, char *));
      break;
    case 'S':
      value->type = STRING;
      char *utf8 = va_arg(ap, char *);
      value->u.as_string = pn_string(utf8);
      break;
    case 'z':
      value->type = BINARY;
      size_t size = va_arg(ap, size_t);
      char *bytes = va_arg(ap, char *);
      value->u.as_binary = pn_binary(bytes, size);
      break;
    case '[':
      stack[level] = (stack_frame_t) {value, scan_size(fmt+1)};
      value->type = LIST;
      value->u.as_list = pn_list(stack[level].count);
      value->u.as_list->size = stack[level].count;
      value = value->u.as_list->values;
      level++;
      continue;
    case ']':
      value = stack[--level].value;
      break;
    case '{':
      stack[level] = (stack_frame_t) {value, scan_size(fmt+1)};
      value->type = MAP;
      value->u.as_map = pn_map(stack[level].count/2);
      value->u.as_map->size = stack[level].count/2;
      value = value->u.as_map->pairs;
      level++;
      continue;
    case '}':
      value = stack[--level].value;
      break;
    case '(':
      // XXX: need to figure out how to detect missing descriptor value here
      // XXX: also, decrementing count may not work when nested
      value--;
      count--;
      pn_tag_t *tag = pn_tag(*value, EMPTY_VALUE);
      value->type = TAG;
      value->u.as_tag = tag;
      stack[level++] = (stack_frame_t) {value, 0};
      value = &value->u.as_tag->value;
      continue;
    case ')':
      value = stack[--level].value;
      break;
    case '@':
      value->type = ARRAY;
      enum TYPE etype = code_to_type(*(++fmt));
      stack[level] = (stack_frame_t) {value, scan_size(++fmt+1)};
      value->u.as_array = pn_array(etype, stack[level].count);
      value->u.as_array->size = stack[level].count;
      value = value->u.as_array->values;
      level++;
      continue;
    default:
      fprintf(stderr, "unrecognized type: %c\n", *fmt);
      return -1;
    }

    value++;
  }

  return count;
}

pn_value_t pn_value(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  pn_value_t value = pn_vvalue(fmt, ap);
  va_end(ap);
  return value;
}

pn_value_t pn_vvalue(const char *fmt, va_list ap)
{
  pn_value_t value;
  pn_vscan(&value, fmt, ap);
  return value;
}

pn_list_t *pn_to_list(pn_value_t v)
{
  return v.u.as_list;
}

pn_map_t *pn_to_map(pn_value_t v)
{
  return v.u.as_map;
}

pn_tag_t *pn_to_tag(pn_value_t v)
{
  return v.u.as_tag;
}

void *pn_to_ref(pn_value_t v)
{
  return v.u.as_ref;
}

pn_value_t pn_from_array(pn_array_t *a)
{
  return (pn_value_t) {.type = ARRAY, .u.as_array = a};
}

pn_value_t pn_from_list(pn_list_t *l)
{
  return (pn_value_t) {.type = LIST, .u.as_list = l};
}

pn_value_t pn_from_map(pn_map_t *m)
{
  return (pn_value_t) {.type = MAP, .u.as_map = m};
}

pn_value_t pn_from_tag(pn_tag_t *t)
{
  return (pn_value_t) {.type = TAG, .u.as_tag = t};
}

pn_value_t pn_from_ref(void *r)
{
  return (pn_value_t) {.type = REF, .u.as_ref = r};
}

pn_value_t pn_from_binary(pn_binary_t *b)
{
  return (pn_value_t) {.type = BINARY, .u.as_binary = b};
}

pn_value_t pn_from_symbol(pn_symbol_t *s)
{
  return (pn_value_t) {.type = SYMBOL, .u.as_symbol = s};
}

int pn_fmt(char **pos, char *limit, const char *fmt, ...)
{
  va_list ap;
  int result;
  char *dst = *pos;
  int n = limit - dst;
  va_start(ap, fmt);
  result = vsnprintf(dst, n, fmt, ap);
  va_end(ap);
  if (result >= n) {
    *pos += n;
    return -1;
  } else if (result >= 0) {
    *pos += result;
    return 0;
  } else {
    // XXX: should convert error codes
    return result;
  }
}

int pn_format_value(char **pos, char *limit, pn_value_t *values, size_t n)
{
  int e;
  for (int i = 0; i < n; i++)
  {
    if (i > 0 && (e = pn_fmt(pos, limit, ", "))) return e;

    pn_value_t v = values[i];
    switch (v.type)
    {
    case EMPTY:
      if ((e = pn_fmt(pos, limit, "NULL"))) return e;
      break;
    case BOOLEAN:
      if (v.u.as_boolean) {
        if ((e = pn_fmt(pos, limit, "true"))) return e;
      } else {
        if ((e = pn_fmt(pos, limit, "false"))) return e;
      }
      break;
    case BYTE:
      if ((e = pn_fmt(pos, limit, "%hhi", v.u.as_byte))) return e;
      break;
    case UBYTE:
      if ((e = pn_fmt(pos, limit, "%hhu", v.u.as_ubyte))) return e;
      break;
    case SHORT:
      if ((e = pn_fmt(pos, limit, "%hi", v.u.as_short))) return e;
      break;
    case USHORT:
      if ((e = pn_fmt(pos, limit, "%hu", v.u.as_ushort))) return e;
      break;
    case INT:
      if ((e = pn_fmt(pos, limit, "%i", v.u.as_int))) return e;
      break;
    case UINT:
      if ((e = pn_fmt(pos, limit, "%u", v.u.as_uint))) return e;
      break;
    case LONG:
      if ((e = pn_fmt(pos, limit, "%lli", v.u.as_long))) return e;
      break;
    case ULONG:
      if ((e = pn_fmt(pos, limit, "%llu", v.u.as_ulong))) return e;
      break;
    case FLOAT:
      if ((e = pn_fmt(pos, limit, "%f", v.u.as_float))) return e;
      break;
    case DOUBLE:
      if ((e = pn_fmt(pos, limit, "%f", v.u.as_double))) return e;
      break;
    case CHAR:
      if ((e = pn_fmt(pos, limit, "%lc", v.u.as_char))) return e;
      break;
    case SYMBOL:
      if ((e = pn_format_symbol(pos, limit, v.u.as_symbol))) return e;
      break;
    case STRING:
      if ((e = pn_fmt(pos, limit, "%s", v.u.as_string->utf8))) return e;
      break;
    case BINARY:
      if ((e = pn_format_binary(pos, limit, v.u.as_binary))) return e;
      break;
    case ARRAY:
      if ((e = pn_format_array(pos, limit, v.u.as_array))) return e;
      break;
    case LIST:
      if ((e = pn_format_list(pos, limit, v.u.as_list))) return e;
      break;
    case MAP:
      if ((e = pn_format_map(pos, limit, v.u.as_map))) return e;
      break;
    case TAG:
      if ((e = pn_format_tag(pos, limit, v.u.as_tag))) return e;
      break;
    case REF:
      if ((e = pn_fmt(pos, limit, "%p", v.u.as_ref))) return e;
      break;
    }
  }

  return 0;
}

int pn_format(char *buf, size_t size, pn_value_t v)
{
  char *pos = buf;
  int n = pn_format_value(&pos, buf + size, &v, 1);
  if (!n) {
    pos[0] = '\0';
  } else {
    if (buf + size - pos < 4) {
      pos = buf + size - 4;
    }
    if (pos > buf) {
      pn_fmt(&pos, buf + size, "...");
      pos[0] = '\0';
    }
  }
  return pos - buf;
}

char *pn_aformat(pn_value_t v)
{
  size_t size = pn_format_sizeof(v) + 1;
  char *buf = malloc(size);
  if (!buf) return NULL;
  char *pos = buf;
  int n = pn_format_value(&pos, buf + size, &v, 1);
  if (!n) {
    pos[0] = '\0';
    return buf;
  } else {
    pn_fatal("bad sizeof");
    free(buf);
    return NULL;
  }
}

size_t pn_format_sizeof(pn_value_t v)
{
  switch (v.type)
  {
  case EMPTY:
    return 4;
  case BOOLEAN:
    return 5;
  case BYTE:
  case UBYTE:
    return 8;
  case SHORT:
  case USHORT:
    return 16;
  case INT:
  case UINT:
  case CHAR:
  case FLOAT:
    return 32;
  case LONG:
  case ULONG:
  case DOUBLE:
    return 64;
  case SYMBOL:
    return v.u.as_symbol->size;
  case STRING:
    return 4*v.u.as_string->size;
  case BINARY:
    return 4*v.u.as_binary->size;
  case ARRAY:
    return pn_format_sizeof_array(v.u.as_array);
  case LIST:
    return pn_format_sizeof_list(v.u.as_list);
  case MAP:
    return pn_format_sizeof_map(v.u.as_map);
  case TAG:
    return pn_format_sizeof_tag(v.u.as_tag);
  case REF:
    return 64;
  default:
    pn_fatal("xxx");
    return 0;
  }
}

// XXX: this should delegate to stuff in codec
size_t pn_encode_sizeof(pn_value_t v)
{
  switch (v.type)
  {
  case EMPTY:
    return 1;
  case BOOLEAN:
  case BYTE:
  case UBYTE:
    return 2;
  case SHORT:
  case USHORT:
    return 3;
  case INT:
  case UINT:
  case CHAR:
  case FLOAT:
    return 5;
  case LONG:
  case ULONG:
  case DOUBLE:
    return 9;
  case SYMBOL:
    return 5 + v.u.as_symbol->size;
  case STRING:
    return 5 + 4*v.u.as_string->size;
  case BINARY:
    return 5 + v.u.as_binary->size;
  case ARRAY:
    return pn_encode_sizeof_array(v.u.as_array);
  case LIST:
    return pn_encode_sizeof_list(v.u.as_list);
  case MAP:
    return pn_encode_sizeof_map(v.u.as_map);
  case TAG:
    return pn_encode_sizeof_tag(v.u.as_tag);
  default:
    pn_fatal("unencodable type: %s", pn_aformat(v));
    return 0;
  }
}

size_t pn_encode(pn_value_t v, char *out)
{
  char *old = out;
  size_t size = pn_encode_sizeof(v);

  switch (v.type)
  {
  case EMPTY:
    pn_write_null(&out, out + size);
    return 1;
  case BOOLEAN:
    pn_write_boolean(&out, out + size, v.u.as_boolean);
    return 2;
  case BYTE:
    pn_write_byte(&out, out + size, v.u.as_byte);
    return 2;
  case UBYTE:
    pn_write_ubyte(&out, out + size, v.u.as_ubyte);
    return 2;
  case SHORT:
    pn_write_short(&out, out + size, v.u.as_short);
    return 3;
  case USHORT:
    pn_write_ushort(&out, out + size, v.u.as_ushort);
    return 3;
  case INT:
    pn_write_int(&out, out + size, v.u.as_int);
    return 5;
  case UINT:
    pn_write_uint(&out, out + size, v.u.as_uint);
    return 5;
  case CHAR:
    pn_write_char(&out, out + size, v.u.as_char);
    return 5;
  case FLOAT:
    pn_write_float(&out, out + size, v.u.as_float);
    return 5;
  case LONG:
    pn_write_long(&out, out + size, v.u.as_long);
    return 9;
  case ULONG:
    pn_write_ulong(&out, out + size, v.u.as_ulong);
    return 9;
  case DOUBLE:
    pn_write_double(&out, out + size, v.u.as_double);
    return 9;
  case SYMBOL:
    pn_write_symbol(&out, out + size, v.u.as_symbol->size, v.u.as_symbol->name);
    return out - old;
  case STRING:
    pn_write_utf8(&out, out + size, v.u.as_string->size, v.u.as_string->utf8);
    return out - old;
  case BINARY:
    pn_write_binary(&out, out + size, v.u.as_binary->size, v.u.as_binary->bytes);
    return 5 + v.u.as_binary->size;
  case ARRAY:
    return pn_encode_array(v.u.as_array, out);
  case LIST:
    return pn_encode_list(v.u.as_list, out);
  case MAP:
    return pn_encode_map(v.u.as_map, out);
  case TAG:
    return pn_encode_tag(v.u.as_tag, out);
  default:
    pn_fatal("unencodable type: %s", pn_aformat(v));
    return 0;
  }
}

void pn_free_value(pn_value_t v)
{
  switch (v.type)
  {
  case EMPTY:
  case BOOLEAN:
  case BYTE:
  case UBYTE:
  case SHORT:
  case USHORT:
  case INT:
  case UINT:
  case CHAR:
  case FLOAT:
  case LONG:
  case ULONG:
  case DOUBLE:
  case REF:
    break;
  case SYMBOL:
    pn_free_symbol(v.u.as_symbol);
    break;
  case STRING:
    pn_free_string(v.u.as_string);
    break;
  case BINARY:
    pn_free_binary(v.u.as_binary);
    break;
  case ARRAY:
    pn_free_array(v.u.as_array);
    break;
  case LIST:
    pn_free_list(v.u.as_list);
    break;
  case MAP:
    pn_free_map(v.u.as_map);
    break;
  case TAG:
    pn_free_tag(v.u.as_tag);
    break;
  }
}

void pn_visit(pn_value_t v, void (*visitor)(pn_value_t))
{
  switch (v.type)
  {
  case EMPTY:
  case BOOLEAN:
  case BYTE:
  case UBYTE:
  case SHORT:
  case USHORT:
  case INT:
  case UINT:
  case CHAR:
  case FLOAT:
  case LONG:
  case ULONG:
  case DOUBLE:
  case STRING:
  case BINARY:
  case REF:
  case SYMBOL:
    break;
  case ARRAY:
    pn_visit_array(v.u.as_array, visitor);
    break;
  case LIST:
    pn_visit_list(v.u.as_list, visitor);
    break;
  case MAP:
    pn_visit_map(v.u.as_map, visitor);
    break;
  case TAG:
    pn_visit_tag(v.u.as_tag, visitor);
    break;
  }

  visitor(v);
}

/* tags */

pn_tag_t *pn_tag(pn_value_t descriptor, pn_value_t value)
{
  pn_tag_t *t = malloc(sizeof(pn_tag_t));
  t->descriptor = descriptor;
  t->value = value;
  return t;
}

void pn_free_tag(pn_tag_t *t)
{
  free(t);
}

void pn_visit_tag(pn_tag_t *t, void (*visitor)(pn_value_t))
{
  pn_visit(t->descriptor, visitor);
  pn_visit(t->value, visitor);
}

pn_value_t pn_tag_descriptor(pn_tag_t *t)
{
  return t->descriptor;
}

pn_value_t pn_tag_value(pn_tag_t *t)
{
  return t->value;
}

size_t pn_format_sizeof_tag(pn_tag_t *tag)
{
  return pn_format_sizeof(tag->descriptor) + pn_format_sizeof(tag->value) + 2;
}

int pn_format_tag(char **pos, char *limit, pn_tag_t *tag)
{
  int e;

  if ((e = pn_format_value(pos, limit, &tag->descriptor, 1))) return e;
  if ((e = pn_fmt(pos, limit, "("))) return e;
  if ((e = pn_format_value(pos, limit, &tag->value, 1))) return e;
  if ((e = pn_fmt(pos, limit, ")"))) return e;

  return 0;
}

size_t pn_encode_sizeof_tag(pn_tag_t *t)
{
  return 1 + pn_encode_sizeof(t->descriptor) + pn_encode_sizeof(t->value);
}

size_t pn_encode_tag(pn_tag_t *t, char *out)
{
  size_t size = 1;
  pn_write_descriptor(&out, out + 1);
  size += pn_encode(t->descriptor, out);
  size += pn_encode(t->value, out + size - 1);
  return size;
}
