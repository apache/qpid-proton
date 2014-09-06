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

#include <proton/object.h>
#include <proton/codec.h>
#include <proton/error.h>
#include <proton/util.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include "encodings.h"
#define DEFINE_FIELDS
#include "protocol.h"
#include "../platform.h"
#include "../platform_fmt.h"
#include "../util.h"
#include "decoder.h"
#include "encoder.h"
#include "data.h"

const char *pn_type_name(pn_type_t type)
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
  case PN_CHAR: return "PN_CHAR";
  case PN_ULONG: return "PN_ULONG";
  case PN_LONG: return "PN_LONG";
  case PN_TIMESTAMP: return "PN_TIMESTAMP";
  case PN_FLOAT: return "PN_FLOAT";
  case PN_DOUBLE: return "PN_DOUBLE";
  case PN_DECIMAL32: return "PN_DECIMAL32";
  case PN_DECIMAL64: return "PN_DECIMAL64";
  case PN_DECIMAL128: return "PN_DECIMAL128";
  case PN_UUID: return "PN_UUID";
  case PN_BINARY: return "PN_BINARY";
  case PN_STRING: return "PN_STRING";
  case PN_SYMBOL: return "PN_SYMBOL";
  case PN_DESCRIBED: return "PN_DESCRIBED";
  case PN_ARRAY: return "PN_ARRAY";
  case PN_LIST: return "PN_LIST";
  case PN_MAP: return "PN_MAP";
  }

  return "<UNKNOWN>";
}

static inline void pni_atom_init(pn_atom_t *atom, pn_type_t type)
{
  memset(atom, 0, sizeof(pn_atom_t));
  atom->type = type;
}

// data

static void pn_data_finalize(void *object)
{
  pn_data_t *data = (pn_data_t *) object;
  free(data->nodes);
  pn_buffer_free(data->buf);
  pn_free(data->str);
  pn_error_free(data->error);
  pn_free(data->decoder);
  pn_free(data->encoder);
}

static const pn_fields_t *pni_node_fields(pn_data_t *data, pni_node_t *node)
{
  if (!node) return NULL;
  if (node->atom.type != PN_DESCRIBED) return NULL;

  pni_node_t *descriptor = pn_data_node(data, node->down);

  if (!descriptor || descriptor->atom.type != PN_ULONG) {
    return NULL;
  }

  if (descriptor->atom.u.as_ulong >= FIELD_MIN && descriptor->atom.u.as_ulong <= FIELD_MAX) {
    const pn_fields_t *f = &FIELDS[descriptor->atom.u.as_ulong-FIELD_MIN];
    return (f->name_index!=0) ? f : NULL;
  } else {
    return NULL;
  }
}

static int pni_node_index(pn_data_t *data, pni_node_t *node)
{
  int count = 0;
  while (node) {
    node = pn_data_node(data, node->prev);
    count++;
  }
  return count - 1;
}

int pni_inspect_atom(pn_atom_t *atom, pn_string_t *str)
{
  switch (atom->type) {
  case PN_NULL:
    return pn_string_addf(str, "null");
  case PN_BOOL:
    return pn_string_addf(str, atom->u.as_bool ? "true" : "false");
  case PN_UBYTE:
    return pn_string_addf(str, "%" PRIu8, atom->u.as_ubyte);
  case PN_BYTE:
    return pn_string_addf(str, "%" PRIi8, atom->u.as_byte);
  case PN_USHORT:
    return pn_string_addf(str, "%" PRIu16, atom->u.as_ushort);
  case PN_SHORT:
    return pn_string_addf(str, "%" PRIi16, atom->u.as_short);
  case PN_UINT:
    return pn_string_addf(str, "%" PRIu32, atom->u.as_uint);
  case PN_INT:
    return pn_string_addf(str, "%" PRIi32, atom->u.as_int);
  case PN_CHAR:
    return pn_string_addf(str, "%lc",  atom->u.as_char);
  case PN_ULONG:
    return pn_string_addf(str, "%" PRIu64, atom->u.as_ulong);
  case PN_LONG:
    return pn_string_addf(str, "%" PRIi64, atom->u.as_long);
  case PN_TIMESTAMP:
    return pn_string_addf(str, "%" PRIi64, atom->u.as_timestamp);
  case PN_FLOAT:
    return pn_string_addf(str, "%g", atom->u.as_float);
  case PN_DOUBLE:
    return pn_string_addf(str, "%g", atom->u.as_double);
  case PN_DECIMAL32:
    return pn_string_addf(str, "D32(%" PRIu32 ")", atom->u.as_decimal32);
  case PN_DECIMAL64:
    return pn_string_addf(str, "D64(%" PRIu64 ")", atom->u.as_decimal64);
  case PN_DECIMAL128:
    return pn_string_addf(str, "D128(%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx"
                          "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx"
                          "%02hhx%02hhx)",
                          atom->u.as_decimal128.bytes[0],
                          atom->u.as_decimal128.bytes[1],
                          atom->u.as_decimal128.bytes[2],
                          atom->u.as_decimal128.bytes[3],
                          atom->u.as_decimal128.bytes[4],
                          atom->u.as_decimal128.bytes[5],
                          atom->u.as_decimal128.bytes[6],
                          atom->u.as_decimal128.bytes[7],
                          atom->u.as_decimal128.bytes[8],
                          atom->u.as_decimal128.bytes[9],
                          atom->u.as_decimal128.bytes[10],
                          atom->u.as_decimal128.bytes[11],
                          atom->u.as_decimal128.bytes[12],
                          atom->u.as_decimal128.bytes[13],
                          atom->u.as_decimal128.bytes[14],
                          atom->u.as_decimal128.bytes[15]);
  case PN_UUID:
    return pn_string_addf(str, "UUID(%02hhx%02hhx%02hhx%02hhx-"
                          "%02hhx%02hhx-%02hhx%02hhx-%02hhx%02hhx-"
                          "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx)",
                          atom->u.as_uuid.bytes[0],
                          atom->u.as_uuid.bytes[1],
                          atom->u.as_uuid.bytes[2],
                          atom->u.as_uuid.bytes[3],
                          atom->u.as_uuid.bytes[4],
                          atom->u.as_uuid.bytes[5],
                          atom->u.as_uuid.bytes[6],
                          atom->u.as_uuid.bytes[7],
                          atom->u.as_uuid.bytes[8],
                          atom->u.as_uuid.bytes[9],
                          atom->u.as_uuid.bytes[10],
                          atom->u.as_uuid.bytes[11],
                          atom->u.as_uuid.bytes[12],
                          atom->u.as_uuid.bytes[13],
                          atom->u.as_uuid.bytes[14],
                          atom->u.as_uuid.bytes[15]);
  case PN_BINARY:
  case PN_STRING:
  case PN_SYMBOL:
    {
      int err;
      const char *pfx;
      pn_bytes_t bin = atom->u.as_bytes;
      bool quote;
      switch (atom->type) {
      case PN_BINARY:
        pfx = "b";
        quote = true;
        break;
      case PN_STRING:
        pfx = "";
        quote = true;
        break;
      case PN_SYMBOL:
        pfx = ":";
        quote = false;
        for (unsigned i = 0; i < bin.size; i++) {
          if (!isalpha(bin.start[i])) {
            quote = true;
            break;
          }
        }
        break;
      default:
        assert(false);
        return PN_ERR;
      }

      if ((err = pn_string_addf(str, "%s", pfx))) return err;
      if (quote) if ((err = pn_string_addf(str, "\""))) return err;
      if ((err = pn_quote(str, bin.start, bin.size))) return err;
      if (quote) if ((err = pn_string_addf(str, "\""))) return err;
      return 0;
    }
  case PN_LIST:
    return pn_string_addf(str, "<list>");
  case PN_MAP:
    return pn_string_addf(str, "<map>");
  case PN_ARRAY:
    return pn_string_addf(str, "<array>");
  case PN_DESCRIBED:
    return pn_string_addf(str, "<described>");
  default:
    return pn_string_addf(str, "<undefined: %i>", atom->type);
  }
}

int pni_inspect_enter(void *ctx, pn_data_t *data, pni_node_t *node)
{
  pn_string_t *str = (pn_string_t *) ctx;
  pn_atom_t *atom = (pn_atom_t *) &node->atom;

  pni_node_t *parent = pn_data_node(data, node->parent);
  const pn_fields_t *fields = pni_node_fields(data, parent);
  pni_node_t *grandparent = parent ? pn_data_node(data, parent->parent) : NULL;
  const pn_fields_t *grandfields = pni_node_fields(data, grandparent);
  int index = pni_node_index(data, node);

  int err;

  if (grandfields) {
    if (atom->type == PN_NULL) {
      return 0;
    }
    const char *name = (index < grandfields->field_count)
        ? FIELD_STRINGPOOL+FIELD_FIELDS[grandfields->first_field_index+index]
        : NULL;
    if (name) {
      err = pn_string_addf(str, "%s=", name);
      if (err) return err;
    }
  }

  switch (atom->type) {
  case PN_DESCRIBED:
    return pn_string_addf(str, "@");
  case PN_ARRAY:
    // XXX: need to fix for described arrays
    return pn_string_addf(str, "@%s[", pn_type_name(node->type));
  case PN_LIST:
    return pn_string_addf(str, "[");
  case PN_MAP:
    return pn_string_addf(str, "{");
  default:
    if (fields && index == 0) {
      err = pn_string_addf(str, "%s", FIELD_STRINGPOOL+FIELD_NAME[fields->name_index]);
      if (err) return err;
      err = pn_string_addf(str, "(");
      if (err) return err;
      err = pni_inspect_atom(atom, str);
      if (err) return err;
      return pn_string_addf(str, ")");
    } else {
      return pni_inspect_atom(atom, str);
    }
  }
}

pni_node_t *pni_next_nonnull(pn_data_t *data, pni_node_t *node)
{
  while (node) {
    node = pn_data_node(data, node->next);
    if (node && node->atom.type != PN_NULL) {
      return node;
    }
  }

  return NULL;
}

int pni_inspect_exit(void *ctx, pn_data_t *data, pni_node_t *node)
{
  pn_string_t *str = (pn_string_t *) ctx;
  pni_node_t *parent = pn_data_node(data, node->parent);
  pni_node_t *grandparent = parent ? pn_data_node(data, parent->parent) : NULL;
  const pn_fields_t *grandfields = pni_node_fields(data, grandparent);
  pni_node_t *next = pn_data_node(data, node->next);
  int err;

  switch (node->atom.type) {
  case PN_ARRAY:
  case PN_LIST:
    err = pn_string_addf(str, "]");
    if (err) return err;
    break;
  case PN_MAP:
    err = pn_string_addf(str, "}");
    if (err) return err;
    break;
  default:
    break;
  }

  if (!grandfields || node->atom.type != PN_NULL) {
    if (next) {
      int index = pni_node_index(data, node);
      if (parent && parent->atom.type == PN_MAP && (index % 2) == 0) {
        err = pn_string_addf(str, "=");
      } else if (parent && parent->atom.type == PN_DESCRIBED && index == 0) {
        err = pn_string_addf(str, " ");
        if (err) return err;
      } else {
        if (!grandfields || pni_next_nonnull(data, node)) {
          err = pn_string_addf(str, ", ");
          if (err) return err;
        }
      }
    }
  }

  return 0;
}

static int pn_data_inspect(void *obj, pn_string_t *dst)
{
  pn_data_t *data = (pn_data_t *) obj;

  return pni_data_traverse(data, pni_inspect_enter, pni_inspect_exit, dst);
}

#define pn_data_initialize NULL
#define pn_data_hashcode NULL
#define pn_data_compare NULL

pn_data_t *pn_data(size_t capacity)
{
  static const pn_class_t clazz = PN_CLASS(pn_data);
  pn_data_t *data = (pn_data_t *) pn_new(sizeof(pn_data_t), &clazz);
  data->capacity = capacity;
  data->size = 0;
  data->nodes = capacity ? (pni_node_t *) malloc(capacity * sizeof(pni_node_t)) : NULL;
  data->buf = pn_buffer(64);
  data->parent = 0;
  data->current = 0;
  data->base_parent = 0;
  data->base_current = 0;
  data->decoder = pn_decoder();
  data->encoder = pn_encoder();
  data->error = pn_error();
  data->str = pn_string(NULL);
  return data;
}

void pn_data_free(pn_data_t *data)
{
  pn_free(data);
}

int pn_data_errno(pn_data_t *data)
{
  return pn_error_code(data->error);
}

pn_error_t *pn_data_error(pn_data_t *data)
{
  return data->error;
}

size_t pn_data_size(pn_data_t *data)
{
  return data ? data->size : 0;
}

void pn_data_clear(pn_data_t *data)
{
  if (data) {
    data->size = 0;
    data->parent = 0;
    data->current = 0;
    data->base_parent = 0;
    data->base_current = 0;
    pn_buffer_clear(data->buf);
  }
}

int pn_data_grow(pn_data_t *data)
{
  data->capacity = 2*(data->capacity ? data->capacity : 2);
  data->nodes = (pni_node_t *) realloc(data->nodes, data->capacity * sizeof(pni_node_t));
  return 0;
}

ssize_t pn_data_intern(pn_data_t *data, const char *start, size_t size)
{
  size_t offset = pn_buffer_size(data->buf);
  int err = pn_buffer_append(data->buf, start, size);
  if (err) return err;
  err = pn_buffer_append(data->buf, "\0", 1);
  if (err) return err;
  return offset;
}

pn_bytes_t *pn_data_bytes(pn_data_t *data, pni_node_t *node)
{
  switch (node->atom.type) {
  case PN_BINARY:
  case PN_STRING:
  case PN_SYMBOL:
    return &node->atom.u.as_bytes;
  default: return NULL;
  }
}

void pn_data_rebase(pn_data_t *data, char *base)
{
  for (unsigned i = 0; i < data->size; i++) {
    pni_node_t *node = &data->nodes[i];
    if (node->data) {
      pn_bytes_t *bytes = pn_data_bytes(data, node);
      bytes->start = base + node->data_offset;
    }
  }
}

int pn_data_intern_node(pn_data_t *data, pni_node_t *node)
{
  pn_bytes_t *bytes = pn_data_bytes(data, node);
  if (!bytes) return 0;
  size_t oldcap = pn_buffer_capacity(data->buf);
  ssize_t offset = pn_data_intern(data, bytes->start, bytes->size);
  if (offset < 0) return offset;
  node->data = true;
  node->data_offset = offset;
  node->data_size = bytes->size;
  pn_buffer_memory_t buf = pn_buffer_memory(data->buf);
  bytes->start = buf.start + offset;

  if (pn_buffer_capacity(data->buf) != oldcap) {
    pn_data_rebase(data, buf.start);
  }

  return 0;
}

int pn_data_vfill(pn_data_t *data, const char *fmt, va_list ap)
{
  int err;
  while (*fmt) {
    char code = *(fmt++);
    if (!code) return 0;

    switch (code) {
    case 'n':
      err = pn_data_put_null(data);
      break;
    case 'o':
      err = pn_data_put_bool(data, va_arg(ap, int));
      break;
    case 'B':
      err = pn_data_put_ubyte(data, va_arg(ap, unsigned int));
      break;
    case 'b':
      err = pn_data_put_byte(data, va_arg(ap, int));
      break;
    case 'H':
      err = pn_data_put_ushort(data, va_arg(ap, unsigned int));
      break;
    case 'h':
      err = pn_data_put_short(data, va_arg(ap, int));
      break;
    case 'I':
      err = pn_data_put_uint(data, va_arg(ap, uint32_t));
      break;
    case 'i':
      err = pn_data_put_int(data, va_arg(ap, uint32_t));
      break;
    case 'L':
      err = pn_data_put_ulong(data, va_arg(ap, uint64_t));
      break;
    case 'l':
      err = pn_data_put_long(data, va_arg(ap, int64_t));
      break;
    case 't':
      err = pn_data_put_timestamp(data, va_arg(ap, pn_timestamp_t));
      break;
    case 'f':
      err = pn_data_put_float(data, va_arg(ap, double));
      break;
    case 'd':
      err = pn_data_put_double(data, va_arg(ap, double));
      break;
    case 'z':
      {
	// For maximum portability, caller must pass these as two separate args, not a single struct
        size_t size = va_arg(ap, size_t);
        char *start = va_arg(ap, char *);
        if (start) {
          err = pn_data_put_binary(data, pn_bytes(size, start));
        } else {
          err = pn_data_put_null(data);
        }
      }
      break;
    case 'S':
    case 's':
      {
        char *start = va_arg(ap, char *);
        size_t size;
        if (start) {
          size = strlen(start);
          if (code == 'S') {
            err = pn_data_put_string(data, pn_bytes(size, start));
          } else {
            err = pn_data_put_symbol(data, pn_bytes(size, start));
          }
        } else {
          err = pn_data_put_null(data);
        }
      }
      break;
    case 'D':
      err = pn_data_put_described(data);
      pn_data_enter(data);
      break;
    case 'T':
      {
        pni_node_t *parent = pn_data_node(data, data->parent);
        if (parent->atom.type == PN_ARRAY) {
          parent->type = (pn_type_t) va_arg(ap, int);
        } else {
          return pn_error_format(data->error, PN_ERR, "naked type");
        }
      }
      break;
    case '@':
      {
        bool described;
        if (*(fmt + 1) == 'D') {
          fmt++;
          described = true;
        } else {
          described = false;
        }
        err = pn_data_put_array(data, described, (pn_type_t) 0);
        pn_data_enter(data);
      }
      break;
    case '[':
      if (*(fmt - 2) != 'T') {
        err = pn_data_put_list(data);
        if (err) return err;
        pn_data_enter(data);
      }
      break;
    case '{':
      err = pn_data_put_map(data);
      if (err) return err;
      pn_data_enter(data);
      break;
    case '}':
    case ']':
      if (!pn_data_exit(data))
        return pn_error_format(data->error, PN_ERR, "exit failed");
      break;
    case '?':
      if (!va_arg(ap, int)) {
        err = pn_data_put_null(data);
        if (err) return err;
        pn_data_enter(data);
      }
      break;
    case '*':
      {
        int count = va_arg(ap, int);
        void *ptr = va_arg(ap, void *);

        char c = *(fmt++);

        switch (c)
        {
        case 's':
          {
            char **sptr = (char **) ptr;
            for (int i = 0; i < count; i++)
            {
              char *sym = *(sptr++);
              err = pn_data_fill(data, "s", sym);
              if (err) return err;
            }
          }
          break;
        default:
          fprintf(stderr, "unrecognized * code: 0x%.2X '%c'\n", code, code);
          return PN_ARG_ERR;
        }
      }
      break;
    case 'C':
      {
        pn_data_t *src = va_arg(ap, pn_data_t *);
        if (src && pn_data_size(src) > 0) {
          err = pn_data_appendn(data, src, 1);
          if (err) return err;
        } else {
          err = pn_data_put_null(data);
          if (err) return err;
        }
      }
      break;
    default:
      fprintf(stderr, "unrecognized fill code: 0x%.2X '%c'\n", code, code);
      return PN_ARG_ERR;
    }

    if (err) return err;

    pni_node_t *parent = pn_data_node(data, data->parent);
    while (parent) {
      if (parent->atom.type == PN_DESCRIBED && parent->children == 2) {
        pn_data_exit(data);
        parent = pn_data_node(data, data->parent);
      } else if (parent->atom.type == PN_NULL && parent->children == 1) {
        pn_data_exit(data);
        pni_node_t *current = pn_data_node(data, data->current);
        current->down = 0;
        current->children = 0;
        if (err) return err;
        parent = pn_data_node(data, data->parent);
      } else {
        break;
      }
    }
  }

  return 0;
}


int pn_data_fill(pn_data_t *data, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int err = pn_data_vfill(data, fmt, ap);
  va_end(ap);
  return err;
}

static bool pn_scan_next(pn_data_t *data, pn_type_t *type, bool suspend)
{
  if (suspend) return false;
  bool found = pn_data_next(data);
  if (found) {
    *type = pn_data_type(data);
    return true;
  } else {
    pni_node_t *parent = pn_data_node(data, data->parent);
    if (parent && parent->atom.type == PN_DESCRIBED) {
      pn_data_exit(data);
      return pn_scan_next(data, type, suspend);
    } else {
      *type = (pn_type_t) -1;
      return false;
    }
  }
}

pni_node_t *pn_data_peek(pn_data_t *data);

int pn_data_vscan(pn_data_t *data, const char *fmt, va_list ap)
{
  pn_data_rewind(data);
  bool *scanarg = NULL;
  bool at = false;
  int level = 0;
  int count_level = -1;
  int resume_count = 0;

  while (*fmt) {
    char code = *(fmt++);

    bool found = false;
    pn_type_t type;

    bool scanned = false;
    bool suspend = resume_count > 0;

    switch (code) {
    case 'n':
      found = pn_scan_next(data, &type, suspend);
      if (found && type == PN_NULL) {
        scanned = true;
      } else {
        scanned = false;
      }
      if (resume_count && level == count_level) resume_count--;
      break;
    case 'o':
      {
        bool *value = va_arg(ap, bool *);
        found = pn_scan_next(data, &type, suspend);
        if (found && type == PN_BOOL) {
          *value = pn_data_get_bool(data);
          scanned = true;
        } else {
          *value = 0;
          scanned = false;
        }
      }
      if (resume_count && level == count_level) resume_count--;
      break;
    case 'B':
      {
        uint8_t *value = va_arg(ap, uint8_t *);
        found = pn_scan_next(data, &type, suspend);
        if (found && type == PN_UBYTE) {
          *value = pn_data_get_ubyte(data);
          scanned = true;
        } else {
          *value = 0;
          scanned = false;
        }
      }
      if (resume_count && level == count_level) resume_count--;
      break;
    case 'b':
      {
        int8_t *value = va_arg(ap, int8_t *);
        found = pn_scan_next(data, &type, suspend);
        if (found && type == PN_BYTE) {
          *value = pn_data_get_byte(data);
          scanned = true;
        } else {
          *value = 0;
          scanned = false;
        }
      }
      if (resume_count && level == count_level) resume_count--;
      break;
    case 'H':
      {
        uint16_t *value = va_arg(ap, uint16_t *);
        found = pn_scan_next(data, &type, suspend);
        if (found && type == PN_USHORT) {
          *value = pn_data_get_ushort(data);
          scanned = true;
        } else {
          *value = 0;
          scanned = false;
        }
      }
      if (resume_count && level == count_level) resume_count--;
      break;
    case 'h':
      {
        int16_t *value = va_arg(ap, int16_t *);
        found = pn_scan_next(data, &type, suspend);
        if (found && type == PN_SHORT) {
          *value = pn_data_get_short(data);
          scanned = true;
        } else {
          *value = 0;
          scanned = false;
        }
      }
      if (resume_count && level == count_level) resume_count--;
      break;
    case 'I':
      {
        uint32_t *value = va_arg(ap, uint32_t *);
        found = pn_scan_next(data, &type, suspend);
        if (found && type == PN_UINT) {
          *value = pn_data_get_uint(data);
          scanned = true;
        } else {
          *value = 0;
          scanned = false;
        }
      }
      if (resume_count && level == count_level) resume_count--;
      break;
    case 'i':
      {
        int32_t *value = va_arg(ap, int32_t *);
        found = pn_scan_next(data, &type, suspend);
        if (found && type == PN_INT) {
          *value = pn_data_get_int(data);
          scanned = true;
        } else {
          *value = 0;
          scanned = false;
        }
      }
      if (resume_count && level == count_level) resume_count--;
      break;
    case 'c':
      {
        pn_char_t *value = va_arg(ap, pn_char_t *);
        found = pn_scan_next(data, &type, suspend);
        if (found && type == PN_CHAR) {
          *value = pn_data_get_char(data);
          scanned = true;
        } else {
          *value = 0;
          scanned = false;
        }
      }
      if (resume_count && level == count_level) resume_count--;
      break;
    case 'L':
      {
        uint64_t *value = va_arg(ap, uint64_t *);
        found = pn_scan_next(data, &type, suspend);
        if (found && type == PN_ULONG) {
          *value = pn_data_get_ulong(data);
          scanned = true;
        } else {
          *value = 0;
          scanned = false;
        }
      }
      if (resume_count && level == count_level) resume_count--;
      break;
    case 'l':
      {
        int64_t *value = va_arg(ap, int64_t *);
        found = pn_scan_next(data, &type, suspend);
        if (found && type == PN_LONG) {
          *value = pn_data_get_long(data);
          scanned = true;
        } else {
          *value = 0;
          scanned = false;
        }
      }
      if (resume_count && level == count_level) resume_count--;
      break;
    case 't':
      {
        pn_timestamp_t *value = va_arg(ap, pn_timestamp_t *);
        found = pn_scan_next(data, &type, suspend);
        if (found && type == PN_TIMESTAMP) {
          *value = pn_data_get_timestamp(data);
          scanned = true;
        } else {
          *value = 0;
          scanned = false;
        }
      }
      if (resume_count && level == count_level) resume_count--;
      break;
    case 'f':
      {
        float *value = va_arg(ap, float *);
        found = pn_scan_next(data, &type, suspend);
        if (found && type == PN_FLOAT) {
          *value = pn_data_get_float(data);
          scanned = true;
        } else {
          *value = 0;
          scanned = false;
        }
      }
      if (resume_count && level == count_level) resume_count--;
      break;
    case 'd':
      {
        double *value = va_arg(ap, double *);
        found = pn_scan_next(data, &type, suspend);
        if (found && type == PN_DOUBLE) {
          *value = pn_data_get_double(data);
          scanned = true;
        } else {
          *value = 0;
          scanned = false;
        }
      }
      if (resume_count && level == count_level) resume_count--;
      break;
    case 'z':
      {
        pn_bytes_t *bytes = va_arg(ap, pn_bytes_t *);
        found = pn_scan_next(data, &type, suspend);
        if (found && type == PN_BINARY) {
          *bytes = pn_data_get_binary(data);
          scanned = true;
        } else {
          bytes->start = 0;
          bytes->size = 0;
          scanned = false;
        }
      }
      if (resume_count && level == count_level) resume_count--;
      break;
    case 'S':
      {
        pn_bytes_t *bytes = va_arg(ap, pn_bytes_t *);
        found = pn_scan_next(data, &type, suspend);
        if (found && type == PN_STRING) {
          *bytes = pn_data_get_string(data);
          scanned = true;
        } else {
          bytes->start = 0;
          bytes->size = 0;
          scanned = false;
        }
      }
      if (resume_count && level == count_level) resume_count--;
      break;
    case 's':
      {
        pn_bytes_t *bytes = va_arg(ap, pn_bytes_t *);
        found = pn_scan_next(data, &type, suspend);
        if (found && type == PN_SYMBOL) {
          *bytes = pn_data_get_symbol(data);
          scanned = true;
        } else {
          bytes->start = 0;
          bytes->size = 0;
          scanned = false;
        }
      }
      if (resume_count && level == count_level) resume_count--;
      break;
    case 'D':
      found = pn_scan_next(data, &type, suspend);
      if (found && type == PN_DESCRIBED) {
        pn_data_enter(data);
        scanned = true;
      } else {
        if (!suspend) {
          resume_count = 3;
          count_level = level;
        }
        scanned = false;
      }
      if (resume_count && level == count_level) resume_count--;
      break;
    case '@':
      found = pn_scan_next(data, &type, suspend);
      if (found && type == PN_ARRAY) {
        pn_data_enter(data);
        scanned = true;
        at = true;
      } else {
        if (!suspend) {
          resume_count = 3;
          count_level = level;
        }
        scanned = false;
      }
      if (resume_count && level == count_level) resume_count--;
      break;
    case '[':
      if (at) {
        scanned = true;
        at = false;
      } else {
        found = pn_scan_next(data, &type, suspend);
        if (found && type == PN_LIST) {
          pn_data_enter(data);
          scanned = true;
        } else {
          if (!suspend) {
            resume_count = 1;
            count_level = level;
          }
          scanned = false;
        }
      }
      level++;
      break;
    case '{':
      found = pn_scan_next(data, &type, suspend);
      if (found && type == PN_MAP) {
        pn_data_enter(data);
        scanned = true;
      } else {
        if (resume_count) {
          resume_count = 1;
          count_level = level;
        }
        scanned = false;
      }
      level++;
      break;
    case ']':
    case '}':
      level--;
      if (!suspend && !pn_data_exit(data))
        return pn_error_format(data->error, PN_ERR, "exit failed");
      if (resume_count && level == count_level) resume_count--;
      break;
    case '.':
      found = pn_scan_next(data, &type, suspend);
      scanned = found;
      if (resume_count && level == count_level) resume_count--;
      break;
    case '?':
      if (!*fmt || *fmt == '?')
        return pn_error_format(data->error, PN_ARG_ERR, "codes must follow a ?");
      scanarg = va_arg(ap, bool *);
      break;
    case 'C':
      {
        pn_data_t *dst = va_arg(ap, pn_data_t *);
        if (!suspend) {
          size_t old = pn_data_size(dst);
          pni_node_t *next = pn_data_peek(data);
          if (next && next->atom.type != PN_NULL) {
            pn_data_narrow(data);
            int err = pn_data_appendn(dst, data, 1);
            pn_data_widen(data);
            if (err) return err;
            scanned = pn_data_size(dst) > old;
          } else {
            scanned = false;
          }
          pn_data_next(data);
        } else {
          scanned = false;
        }
      }
      if (resume_count && level == count_level) resume_count--;
      break;
    default:
      return pn_error_format(data->error, PN_ARG_ERR, "unrecognized scan code: 0x%.2X '%c'", code, code);
    }

    if (scanarg && code != '?') {
      *scanarg = scanned;
      scanarg = NULL;
    }
  }

  return 0;
}

int pn_data_scan(pn_data_t *data, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int err = pn_data_vscan(data, fmt, ap);
  va_end(ap);
  return err;
}

static int pni_data_inspectify(pn_data_t *data)
{
  int err = pn_string_set(data->str, "");
  if (err) return err;
  return pn_data_inspect(data, data->str);
}

int pn_data_print(pn_data_t *data)
{
  int err = pni_data_inspectify(data);
  if (err) return err;
  printf("%s", pn_string_get(data->str));
  return 0;
}

int pn_data_format(pn_data_t *data, char *bytes, size_t *size)
{
  int err = pni_data_inspectify(data);
  if (err) return err;
  if (pn_string_size(data->str) >= *size) {
    return PN_OVERFLOW;
  } else {
    pn_string_put(data->str, bytes);
    *size = pn_string_size(data->str);
    return 0;
  }
}

int pn_data_resize(pn_data_t *data, size_t size)
{
  if (!data || size > data->capacity) return PN_ARG_ERR;
  data->size = size;
  return 0;
}


pni_node_t *pn_data_node(pn_data_t *data, pni_nid_t nd)
{
  if (nd) {
    return &data->nodes[nd - 1];
  } else {
    return NULL;
  }
}

size_t pn_data_id(pn_data_t *data, pni_node_t *node)
{
  return node - data->nodes + 1;
}

pni_node_t *pn_data_new(pn_data_t *data)
{
  if (data->capacity <= data->size) {
    pn_data_grow(data);
  }
  pni_node_t *node = pn_data_node(data, ++(data->size));
  node->next = 0;
  node->down = 0;
  node->children = 0;
  return node;
}

void pn_data_rewind(pn_data_t *data)
{
  data->parent = data->base_parent;
  data->current = data->base_current;
}

pni_node_t *pn_data_current(pn_data_t *data)
{
  return pn_data_node(data, data->current);
}

void pn_data_narrow(pn_data_t *data)
{
  data->base_parent = data->parent;
  data->base_current = data->current;
}

void pn_data_widen(pn_data_t *data)
{
  data->base_parent = 0;
  data->base_current = 0;
}

pn_handle_t pn_data_point(pn_data_t *data)
{
  if (data->current) {
    return data->current;
  } else {
    return -data->parent;
  }
}

bool pn_data_restore(pn_data_t *data, pn_handle_t point)
{
  pn_shandle_t spoint = (pn_shandle_t) point;
  if (spoint < 0 && ((size_t) (-spoint)) <= data->size) {
    data->parent = -((pn_shandle_t) point);
    data->current = 0;
    return true;
  } else if (point && point <= data->size) {
    data->current = point;
    pni_node_t *current = pn_data_current(data);
    data->parent = current->parent;
    return true;
  } else {
    return false;
  }
}

pni_node_t *pn_data_peek(pn_data_t *data)
{
  pni_node_t *current = pn_data_current(data);
  if (current) {
    return pn_data_node(data, current->next);
  }

  pni_node_t *parent = pn_data_node(data, data->parent);
  if (parent) {
    return pn_data_node(data, parent->down);
  }

  return NULL;
}

bool pn_data_next(pn_data_t *data)
{
  pni_node_t *current = pn_data_current(data);
  pni_node_t *parent = pn_data_node(data, data->parent);
  size_t next;

  if (current) {
    next = current->next;
  } else if (parent && parent->down) {
    next = parent->down;
  } else if (!parent && data->size) {
    next = 1;
  } else {
    return false;
  }

  if (next) {
    data->current = next;
    return true;
  } else {
    return false;
  }
}

bool pn_data_prev(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node && node->prev) {
    data->current = node->prev;
    return true;
  } else {
    return false;
  }
}

int pni_data_traverse(pn_data_t *data,
                      int (*enter)(void *ctx, pn_data_t *data, pni_node_t *node),
                      int (*exit)(void *ctx, pn_data_t *data, pni_node_t *node),
                      void *ctx)
{
  pni_node_t *node = data->size ? pn_data_node(data, 1) : NULL;
  while (node) {
    pni_node_t *parent = pn_data_node(data, node->parent);

    int err = enter(ctx, data, node);
    if (err) return err;

    size_t next = 0;
    if (node->down) {
      next = node->down;
    } else if (node->next) {
      err = exit(ctx, data, node);
      if (err) return err;
      next = node->next;
    } else {
      err = exit(ctx, data, node);
      if (err) return err;
      while (parent) {
        err = exit(ctx, data, parent);
        if (err) return err;
        if (parent->next) {
          next = parent->next;
          break;
        } else {
          parent = pn_data_node(data, parent->parent);
        }
      }
    }

    node = pn_data_node(data, next);
  }

  return 0;
}

pn_type_t pn_data_type(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node) {
    return node->atom.type;
  } else {
    return (pn_type_t) -1;
  }
}

pn_type_t pni_data_parent_type(pn_data_t *data)
{
  pni_node_t *node = pn_data_node(data, data->parent);
  if (node) {
    return node->atom.type;
  } else {
    return (pn_type_t) -1;
  }
}

size_t pn_data_siblings(pn_data_t *data)
{
  pni_node_t *node = pn_data_node(data, data->parent);
  if (node) {
    return node->children;
  } else {
    return 0;
  }
}

bool pn_data_enter(pn_data_t *data)
{
  if (data->current) {
    data->parent = data->current;
    data->current = 0;
    return true;
  } else {
    return false;
  }
}

bool pn_data_exit(pn_data_t *data)
{
  if (data->parent) {
    pni_node_t *parent = pn_data_node(data, data->parent);
    data->current = data->parent;
    data->parent = parent->parent;
    return true;
  } else {
    return false;
  }
}

bool pn_data_lookup(pn_data_t *data, const char *name)
{
  while (pn_data_next(data)) {
    pn_type_t type = pn_data_type(data);

    switch (type) {
    case PN_STRING:
    case PN_SYMBOL:
      {
        pn_bytes_t bytes = pn_data_get_bytes(data);
        if (!strncmp(name, bytes.start, bytes.size)) {
          return pn_data_next(data);
        }
      }
      break;
    default:
      break;
    }

    // skip the value
    pn_data_next(data);
  }

  return false;
}

void pn_data_dump(pn_data_t *data)
{
  printf("{current=%" PN_ZI ", parent=%" PN_ZI "}\n", (size_t) data->current, (size_t) data->parent);
  for (unsigned i = 0; i < data->size; i++)
  {
    pni_node_t *node = &data->nodes[i];
    pn_string_set(data->str, "");
    pni_inspect_atom((pn_atom_t *) &node->atom, data->str);
    printf("Node %i: prev=%" PN_ZI ", next=%" PN_ZI ", parent=%" PN_ZI ", down=%" PN_ZI 
           ", children=%" PN_ZI ", type=%s (%s)\n",
           i + 1, (size_t) node->prev,
           (size_t) node->next,
           (size_t) node->parent,
           (size_t) node->down,
           (size_t) node->children,
           pn_type_name(node->atom.type), pn_string_get(data->str));
  }
}

pni_node_t *pn_data_add(pn_data_t *data)
{
  pni_node_t *current = pn_data_current(data);
  pni_node_t *parent = pn_data_node(data, data->parent);
  pni_node_t *node;

  if (current) {
    if (current->next) {
      node = pn_data_node(data, current->next);
    } else {
      node = pn_data_new(data);
      // refresh the pointers in case we grew
      current = pn_data_current(data);
      parent = pn_data_node(data, data->parent);
      node->prev = data->current;
      current->next = pn_data_id(data, node);
      node->parent = data->parent;
      if (parent) {
        if (!parent->down) {
          parent->down = pn_data_id(data, node);
        }
        parent->children++;
      }
    }
  } else if (parent) {
    if (parent->down) {
      node = pn_data_node(data, parent->down);
    } else {
      node = pn_data_new(data);
      // refresh the pointers in case we grew
      parent = pn_data_node(data, data->parent);
      node->prev = 0;
      node->parent = data->parent;
      parent->down = pn_data_id(data, node);
      parent->children++;
    }
  } else if (data->size) {
    node = pn_data_node(data, 1);
  } else {
    node = pn_data_new(data);
    node->prev = 0;
    node->parent = 0;
  }

  node->down = 0;
  node->children = 0;
  node->data = false;
  node->data_offset = 0;
  node->data_size = 0;
  data->current = pn_data_id(data, node);
  return node;
}

ssize_t pn_data_encode(pn_data_t *data, char *bytes, size_t size)
{
  return pn_encoder_encode(data->encoder, data, bytes, size);
}

ssize_t pn_data_decode(pn_data_t *data, const char *bytes, size_t size)
{
  return pn_decoder_decode(data->decoder, bytes, size, data);
}

int pn_data_put_list(pn_data_t *data)
{
  pni_node_t *node = pn_data_add(data);
  node->atom.type = PN_LIST;
  return 0;
}

int pn_data_put_map(pn_data_t *data)
{
  pni_node_t *node = pn_data_add(data);
  node->atom.type = PN_MAP;
  return 0;
}

int pn_data_put_array(pn_data_t *data, bool described, pn_type_t type)
{
  pni_node_t *node = pn_data_add(data);
  node->atom.type = PN_ARRAY;
  node->described = described;
  node->type = type;
  return 0;
}

void pni_data_set_array_type(pn_data_t *data, pn_type_t type)
{
  pni_node_t *array = pn_data_current(data);
  array->type = type;
}

int pn_data_put_described(pn_data_t *data)
{
  pni_node_t *node = pn_data_add(data);
  node->atom.type = PN_DESCRIBED;
  return 0;
}

int pn_data_put_null(pn_data_t *data)
{
  pni_node_t *node = pn_data_add(data);
  pni_atom_init(&node->atom, PN_NULL);
  return 0;
}

int pn_data_put_bool(pn_data_t *data, bool b)
{
  pni_node_t *node = pn_data_add(data);
  node->atom.type = PN_BOOL;
  node->atom.u.as_bool = b;
  return 0;
}

int pn_data_put_ubyte(pn_data_t *data, uint8_t ub)
{
  pni_node_t *node = pn_data_add(data);
  node->atom.type = PN_UBYTE;
  node->atom.u.as_ubyte = ub;
  return 0;
}

int pn_data_put_byte(pn_data_t *data, int8_t b)
{
  pni_node_t *node = pn_data_add(data);
  node->atom.type = PN_BYTE;
  node->atom.u.as_byte = b;
  return 0;
}

int pn_data_put_ushort(pn_data_t *data, uint16_t us)
{
  pni_node_t *node = pn_data_add(data);
  node->atom.type = PN_USHORT;
  node->atom.u.as_ushort = us;
  return 0;
}

int pn_data_put_short(pn_data_t *data, int16_t s)
{
  pni_node_t *node = pn_data_add(data);
  node->atom.type = PN_SHORT;
  node->atom.u.as_short = s;
  return 0;
}

int pn_data_put_uint(pn_data_t *data, uint32_t ui)
{
  pni_node_t *node = pn_data_add(data);
  node->atom.type = PN_UINT;
  node->atom.u.as_uint = ui;
  return 0;
}

int pn_data_put_int(pn_data_t *data, int32_t i)
{
  pni_node_t *node = pn_data_add(data);
  node->atom.type = PN_INT;
  node->atom.u.as_int = i;
  return 0;
}

int pn_data_put_char(pn_data_t *data, pn_char_t c)
{
  pni_node_t *node = pn_data_add(data);
  node->atom.type = PN_CHAR;
  node->atom.u.as_char = c;
  return 0;
}

int pn_data_put_ulong(pn_data_t *data, uint64_t ul)
{
  pni_node_t *node = pn_data_add(data);
  node->atom.type = PN_ULONG;
  node->atom.u.as_ulong = ul;
  return 0;
}

int pn_data_put_long(pn_data_t *data, int64_t l)
{
  pni_node_t *node = pn_data_add(data);
  node->atom.type = PN_LONG;
  node->atom.u.as_long = l;
  return 0;
}

int pn_data_put_timestamp(pn_data_t *data, pn_timestamp_t t)
{
  pni_node_t *node = pn_data_add(data);
  node->atom.type = PN_TIMESTAMP;
  node->atom.u.as_timestamp = t;
  return 0;
}

int pn_data_put_float(pn_data_t *data, float f)
{
  pni_node_t *node = pn_data_add(data);
  node->atom.type = PN_FLOAT;
  node->atom.u.as_float = f;
  return 0;
}

int pn_data_put_double(pn_data_t *data, double d)
{
  pni_node_t *node = pn_data_add(data);
  node->atom.type = PN_DOUBLE;
  node->atom.u.as_double = d;
  return 0;
}

int pn_data_put_decimal32(pn_data_t *data, pn_decimal32_t d)
{
  pni_node_t *node = pn_data_add(data);
  node->atom.type = PN_DECIMAL32;
  node->atom.u.as_decimal32 = d;
  return 0;
}

int pn_data_put_decimal64(pn_data_t *data, pn_decimal64_t d)
{
  pni_node_t *node = pn_data_add(data);
  node->atom.type = PN_DECIMAL64;
  node->atom.u.as_decimal64 = d;
  return 0;
}

int pn_data_put_decimal128(pn_data_t *data, pn_decimal128_t d)
{
  pni_node_t *node = pn_data_add(data);
  node->atom.type = PN_DECIMAL128;
  memmove(node->atom.u.as_decimal128.bytes, d.bytes, 16);
  return 0;
}

int pn_data_put_uuid(pn_data_t *data, pn_uuid_t u)
{
  pni_node_t *node = pn_data_add(data);
  node->atom.type = PN_UUID;
  memmove(node->atom.u.as_uuid.bytes, u.bytes, 16);
  return 0;
}

int pn_data_put_binary(pn_data_t *data, pn_bytes_t bytes)
{
  pni_node_t *node = pn_data_add(data);
  node->atom.type = PN_BINARY;
  node->atom.u.as_bytes = bytes;
  return pn_data_intern_node(data, node);
}

int pn_data_put_string(pn_data_t *data, pn_bytes_t string)
{
  pni_node_t *node = pn_data_add(data);
  node->atom.type = PN_STRING;
  node->atom.u.as_bytes = string;
  return pn_data_intern_node(data, node);
}

int pn_data_put_symbol(pn_data_t *data, pn_bytes_t symbol)
{
  pni_node_t *node = pn_data_add(data);
  node->atom.type = PN_SYMBOL;
  node->atom.u.as_bytes = symbol;
  return pn_data_intern_node(data, node);
}

int pn_data_put_atom(pn_data_t *data, pn_atom_t atom)
{
  pni_node_t *node = pn_data_add(data);
  node->atom = atom;
  return pn_data_intern_node(data, node);
}

size_t pn_data_get_list(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node && node->atom.type == PN_LIST) {
    return node->children;
  } else {
    return 0;
  }
}

size_t pn_data_get_map(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node && node->atom.type == PN_MAP) {
    return node->children;
  } else {
    return 0;
  }
}

size_t pn_data_get_array(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node && node->atom.type == PN_ARRAY) {
    if (node->described) {
      return node->children - 1;
    } else {
      return node->children;
    }
  } else {
    return 0;
  }
}

bool pn_data_is_array_described(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node && node->atom.type == PN_ARRAY) {
    return node->described;
  } else {
    return false;
  }
}

pn_type_t pn_data_get_array_type(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node && node->atom.type == PN_ARRAY) {
    return node->type;
  } else {
    return (pn_type_t) -1;
  }
}

bool pn_data_is_described(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  return node && node->atom.type == PN_DESCRIBED;
}

bool pn_data_is_null(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  return node && node->atom.type == PN_NULL;
}

bool pn_data_get_bool(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node && node->atom.type == PN_BOOL) {
    return node->atom.u.as_bool;
  } else {
    return false;
  }
}

uint8_t pn_data_get_ubyte(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node && node->atom.type == PN_UBYTE) {
    return node->atom.u.as_ubyte;
  } else {
    return 0;
  }
}

int8_t pn_data_get_byte(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node && node->atom.type == PN_BYTE) {
    return node->atom.u.as_byte;
  } else {
    return 0;
  }
}

uint16_t pn_data_get_ushort(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node && node->atom.type == PN_USHORT) {
    return node->atom.u.as_ushort;
  } else {
    return 0;
  }
}

int16_t pn_data_get_short(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node && node->atom.type == PN_SHORT) {
    return node->atom.u.as_short;
  } else {
    return 0;
  }
}

uint32_t pn_data_get_uint(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node && node->atom.type == PN_UINT) {
    return node->atom.u.as_uint;
  } else {
    return 0;
  }
}

int32_t pn_data_get_int(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node && node->atom.type == PN_INT) {
    return node->atom.u.as_int;
  } else {
    return 0;
  }
}

pn_char_t pn_data_get_char(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node && node->atom.type == PN_CHAR) {
    return node->atom.u.as_char;
  } else {
    return 0;
  }
}

uint64_t pn_data_get_ulong(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node && node->atom.type == PN_ULONG) {
    return node->atom.u.as_ulong;
  } else {
    return 0;
  }
}

int64_t pn_data_get_long(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node && node->atom.type == PN_LONG) {
    return node->atom.u.as_long;
  } else {
    return 0;
  }
}

pn_timestamp_t pn_data_get_timestamp(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node && node->atom.type == PN_TIMESTAMP) {
    return node->atom.u.as_timestamp;
  } else {
    return 0;
  }
}

float pn_data_get_float(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node && node->atom.type == PN_FLOAT) {
    return node->atom.u.as_float;
  } else {
    return 0;
  }
}

double pn_data_get_double(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node && node->atom.type == PN_DOUBLE) {
    return node->atom.u.as_double;
  } else {
    return 0;
  }
}

pn_decimal32_t pn_data_get_decimal32(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node && node->atom.type == PN_DECIMAL32) {
    return node->atom.u.as_decimal32;
  } else {
    return 0;
  }
}

pn_decimal64_t pn_data_get_decimal64(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node && node->atom.type == PN_DECIMAL64) {
    return node->atom.u.as_decimal64;
  } else {
    return 0;
  }
}

pn_decimal128_t pn_data_get_decimal128(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node && node->atom.type == PN_DECIMAL128) {
    return node->atom.u.as_decimal128;
  } else {
    pn_decimal128_t t = {{0}};
    return t;
  }
}

pn_uuid_t pn_data_get_uuid(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node && node->atom.type == PN_UUID) {
    return node->atom.u.as_uuid;
  } else {
    pn_uuid_t t = {{0}};
    return t;
  }
}

pn_bytes_t pn_data_get_binary(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node && node->atom.type == PN_BINARY) {
    return node->atom.u.as_bytes;
  } else {
    pn_bytes_t t = {0};
    return t;
  }
}

pn_bytes_t pn_data_get_string(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node && node->atom.type == PN_STRING) {
    return node->atom.u.as_bytes;
  } else {
    pn_bytes_t t = {0};
    return t;
  }
}

pn_bytes_t pn_data_get_symbol(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node && node->atom.type == PN_SYMBOL) {
    return node->atom.u.as_bytes;
  } else {
    pn_bytes_t t = {0};
    return t;
  }
}

pn_bytes_t pn_data_get_bytes(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node && (node->atom.type == PN_BINARY ||
               node->atom.type == PN_STRING ||
               node->atom.type == PN_SYMBOL)) {
    return node->atom.u.as_bytes;
  } else {
    pn_bytes_t t = {0};
    return t;
  }
}

pn_atom_t pn_data_get_atom(pn_data_t *data)
{
  pni_node_t *node = pn_data_current(data);
  if (node) {
    return *((pn_atom_t *) &node->atom);
  } else {
    pn_atom_t t = {PN_NULL};
    return t;
  }
}

int pn_data_copy(pn_data_t *data, pn_data_t *src)
{
  pn_data_clear(data);
  int err = pn_data_append(data, src);
  pn_data_rewind(data);
  return err;
}

int pn_data_append(pn_data_t *data, pn_data_t *src)
{
  return pn_data_appendn(data, src, -1);
}

int pn_data_appendn(pn_data_t *data, pn_data_t *src, int limit)
{
  int err = 0;
  int level = 0, count = 0;
  bool stop = false;
  pn_handle_t point = pn_data_point(src);
  pn_data_rewind(src);

  while (true) {
    while (!pn_data_next(src)) {
      if (level > 0) {
        pn_data_exit(data);
        pn_data_exit(src);
        level--;
        continue;
      }

      if (!pn_data_next(src)) {
        stop = true;
      }
      break;
    }

    if (stop) break;

    if (level == 0 && count == limit)
      break;

    pn_type_t type = pn_data_type(src);
    switch (type) {
    case PN_NULL:
      err = pn_data_put_null(data);
      if (level == 0) count++;
      break;
    case PN_BOOL:
      err = pn_data_put_bool(data, pn_data_get_bool(src));
      if (level == 0) count++;
      break;
    case PN_UBYTE:
      err = pn_data_put_ubyte(data, pn_data_get_ubyte(src));
      if (level == 0) count++;
      break;
    case PN_BYTE:
      err = pn_data_put_byte(data, pn_data_get_byte(src));
      if (level == 0) count++;
      break;
    case PN_USHORT:
      err = pn_data_put_ushort(data, pn_data_get_ushort(src));
      if (level == 0) count++;
      break;
    case PN_SHORT:
      err = pn_data_put_short(data, pn_data_get_short(src));
      if (level == 0) count++;
      break;
    case PN_UINT:
      err = pn_data_put_uint(data, pn_data_get_uint(src));
      if (level == 0) count++;
      break;
    case PN_INT:
      err = pn_data_put_int(data, pn_data_get_int(src));
      if (level == 0) count++;
      break;
    case PN_CHAR:
      err = pn_data_put_char(data, pn_data_get_char(src));
      if (level == 0) count++;
      break;
    case PN_ULONG:
      err = pn_data_put_ulong(data, pn_data_get_ulong(src));
      if (level == 0) count++;
      break;
    case PN_LONG:
      err = pn_data_put_long(data, pn_data_get_long(src));
      if (level == 0) count++;
      break;
    case PN_TIMESTAMP:
      err = pn_data_put_timestamp(data, pn_data_get_timestamp(src));
      if (level == 0) count++;
      break;
    case PN_FLOAT:
      err = pn_data_put_float(data, pn_data_get_float(src));
      if (level == 0) count++;
      break;
    case PN_DOUBLE:
      err = pn_data_put_double(data, pn_data_get_double(src));
      if (level == 0) count++;
      break;
    case PN_DECIMAL32:
      err = pn_data_put_decimal32(data, pn_data_get_decimal32(src));
      if (level == 0) count++;
      break;
    case PN_DECIMAL64:
      err = pn_data_put_decimal64(data, pn_data_get_decimal64(src));
      if (level == 0) count++;
      break;
    case PN_DECIMAL128:
      err = pn_data_put_decimal128(data, pn_data_get_decimal128(src));
      if (level == 0) count++;
      break;
    case PN_UUID:
      err = pn_data_put_uuid(data, pn_data_get_uuid(src));
      if (level == 0) count++;
      break;
    case PN_BINARY:
      err = pn_data_put_binary(data, pn_data_get_binary(src));
      if (level == 0) count++;
      break;
    case PN_STRING:
      err = pn_data_put_string(data, pn_data_get_string(src));
      if (level == 0) count++;
      break;
    case PN_SYMBOL:
      err = pn_data_put_symbol(data, pn_data_get_symbol(src));
      if (level == 0) count++;
      break;
    case PN_DESCRIBED:
      err = pn_data_put_described(data);
      if (level == 0) count++;
      if (err) { pn_data_restore(src, point); return err; }
      pn_data_enter(data);
      pn_data_enter(src);
      level++;
      break;
    case PN_ARRAY:
      err = pn_data_put_array(data, pn_data_is_array_described(src),
                              pn_data_get_array_type(src));
      if (level == 0) count++;
      if (err) { pn_data_restore(src, point); return err; }
      pn_data_enter(data);
      pn_data_enter(src);
      level++;
      break;
    case PN_LIST:
      err = pn_data_put_list(data);
      if (level == 0) count++;
      if (err) { pn_data_restore(src, point); return err; }
      pn_data_enter(data);
      pn_data_enter(src);
      level++;
      break;
    case PN_MAP:
      err = pn_data_put_map(data);
      if (level == 0) count++;
      if (err) { pn_data_restore(src, point); return err; }
      pn_data_enter(data);
      pn_data_enter(src);
      level++;
      break;
    }

    if (err) { pn_data_restore(src, point); return err; }
  }

  pn_data_restore(src, point);

  return 0;
}
