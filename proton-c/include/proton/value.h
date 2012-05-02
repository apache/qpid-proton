#ifndef _PROTON_VALUE_H
#define _PROTON_VALUE_H 1

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

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

enum TYPE {
  EMPTY,
  BOOLEAN,
  UBYTE,
  USHORT,
  UINT,
  ULONG,
  BYTE,
  SHORT,
  INT,
  LONG,
  FLOAT,
  DOUBLE,
  CHAR,
  SYMBOL,
  STRING,
  BINARY,
  ARRAY,
  LIST,
  MAP,
  TAG,
  REF
};

typedef struct pn_value_t pn_value_t;
typedef struct pn_symbol_t pn_symbol_t;
typedef struct pn_string_t pn_string_t;
typedef struct pn_binary_t pn_binary_t;
typedef struct pn_array_t pn_array_t;
typedef struct pn_list_t pn_list_t;
typedef struct pn_map_t pn_map_t;
typedef struct pn_tag_t pn_tag_t;

struct  pn_value_t {
  enum TYPE type;
  union {
    bool as_boolean;
    uint8_t as_ubyte;
    uint16_t as_ushort;
    uint32_t as_uint;
    uint64_t as_ulong;
    int8_t as_byte;
    int16_t as_short;
    int32_t as_int;
    int64_t as_long;
    float as_float;
    double as_double;
    wchar_t as_char;
    pn_symbol_t *as_symbol;
    pn_string_t *as_string;
    pn_binary_t *as_binary;
    pn_array_t *as_array;
    pn_list_t *as_list;
    pn_map_t *as_map;
    pn_tag_t *as_tag;
    void *as_ref;
  } u;
};

struct pn_tag_t {
  pn_value_t descriptor;
  pn_value_t value;
};

#define EMPTY_VALUE ((pn_value_t) {.type = EMPTY, .u = {0}})

int pn_scan(pn_value_t *value, const char *fmt, ...);
int pn_vscan(pn_value_t *value, const char *fmt, va_list ap);
pn_value_t pn_value(const char *fmt, ...);
pn_value_t pn_vvalue(const char *fmt, va_list ap);

pn_list_t *pn_to_list(pn_value_t v);
pn_map_t *pn_to_map(pn_value_t v);
pn_tag_t *pn_to_tag(pn_value_t v);
void *pn_to_ref(pn_value_t v);

pn_value_t pn_from_array(pn_array_t *a);
pn_value_t pn_from_list(pn_list_t *l);
pn_value_t pn_from_map(pn_map_t *m);
pn_value_t pn_from_tag(pn_tag_t *t);
pn_value_t pn_from_ref(void *r);
pn_value_t pn_from_binary(pn_binary_t *b);
pn_value_t pn_from_symbol(pn_symbol_t *s);

int pn_compare_symbol(pn_symbol_t *a, pn_symbol_t *b);
int pn_compare_string(pn_string_t *a, pn_string_t *b);
int pn_compare_binary(pn_binary_t *a, pn_binary_t *b);
int pn_compare_list(pn_list_t *a, pn_list_t *b);
int pn_compare_map(pn_map_t *a, pn_map_t *b);
int pn_compare_tag(pn_tag_t *a, pn_tag_t *b);
int pn_compare_value(pn_value_t a, pn_value_t b);

uintptr_t pn_hash_string(pn_string_t *s);
uintptr_t pn_hash_binary(pn_binary_t *b);
uintptr_t pn_hash_list(pn_list_t *l);
uintptr_t pn_hash_map(pn_map_t *m);
uintptr_t pn_hash_tag(pn_tag_t *t);
uintptr_t pn_hash_value(pn_value_t v);

size_t pn_format_sizeof(pn_value_t v);
size_t pn_format_sizeof_array(pn_array_t *array);
size_t pn_format_sizeof_list(pn_list_t *list);
size_t pn_format_sizeof_map(pn_map_t *map);
size_t pn_format_sizeof_tag(pn_tag_t *tag);

int pn_format_symbol(char **pos, char *limit, pn_symbol_t *sym);
int pn_format_binary(char **pos, char *limit, pn_binary_t *binary);
int pn_format_array(char **pos, char *limit, pn_array_t *array);
int pn_format_list(char **pos, char *limit, pn_list_t *list);
int pn_format_map(char **pos, char *limit, pn_map_t *map);
int pn_format_tag(char **pos, char *limit, pn_tag_t *tag);
int pn_format_value(char **pos, char *limit, pn_value_t *values, size_t n);
int pn_format(char *buf, size_t size, pn_value_t v);
char *pn_aformat(pn_value_t v);

size_t pn_encode_sizeof(pn_value_t v);
size_t pn_encode(pn_value_t v, char *out);
ssize_t pn_decode(pn_value_t *v, char *bytes, size_t n);

void pn_free_value(pn_value_t v);
void pn_free_array(pn_array_t *a);
void pn_free_list(pn_list_t *l);
void pn_free_map(pn_map_t *m);
void pn_free_tag(pn_tag_t *t);
void pn_free_symbol(pn_symbol_t *s);
void pn_free_binary(pn_binary_t *b);
void pn_free_string(pn_string_t *s);

void pn_visit(pn_value_t v, void (*visitor)(pn_value_t));
void pn_visit_array(pn_array_t *v, void (*visitor)(pn_value_t));
void pn_visit_list(pn_list_t *l, void (*visitor)(pn_value_t));
void pn_visit_map(pn_map_t *m, void (*visitor)(pn_value_t));
void pn_visit_tag(pn_tag_t *t, void (*visitor)(pn_value_t));

/* scalars */
#define pn_boolean(V) ((pn_value_t) {.type = BOOLEAN, .u.as_boolean = (V)})
#define pn_uint(V) ((pn_value_t) {.type = UINT, .u.as_uint = (V)})
#define pn_ulong(V) ((pn_value_t) {.type = ULONG, .u.as_ulong = (V)})
#define pn_to_uint8(V) ((V).u.as_ubyte)
#define pn_to_uint16(V) ((V).u.as_ushort)
#define pn_to_uint32(V) ((V).u.as_uint)
#define pn_to_int32(V) ((V).u.as_int)
#define pn_to_bool(V) ((V).u.as_boolean)
#define pn_to_string(V) ((V).u.as_string)
#define pn_to_binary(V) ((V).u.as_binary)
#define pn_to_symbol(V) ((V).u.as_symbol)

/* symbol */
pn_symbol_t *pn_symbol(const char *name);
pn_symbol_t *pn_symboln(const char *name, size_t size);
size_t pn_symbol_size(pn_symbol_t *s);
const char *pn_symbol_name(pn_symbol_t *s);
pn_symbol_t *pn_symbol_dup(pn_symbol_t *s);

/* string */

pn_string_t *pn_string(const char *utf8);
pn_string_t *pn_stringn(const char *utf8, size_t size);
const char *pn_string_utf8(pn_string_t *str);

/* binary */

pn_binary_t *pn_binary(const char *bytes, size_t size);
size_t pn_binary_size(pn_binary_t *b);
const char *pn_binary_bytes(pn_binary_t *b);
pn_binary_t *pn_binary_dup(pn_binary_t *b);
ssize_t pn_binary_get(pn_binary_t *b, char *bytes, size_t size);

/* arrays */

pn_array_t *pn_array(enum TYPE type, int capacity);
int pn_array_add(pn_array_t *a, pn_value_t v);
pn_value_t pn_array_get(pn_array_t *a, int index);
size_t pn_encode_sizeof_array(pn_array_t *a);
size_t pn_encode_array(pn_array_t *array, char *out);

/* lists */

pn_list_t *pn_list(int capacity);
pn_value_t pn_list_get(pn_list_t *l, int index);
pn_value_t pn_list_set(pn_list_t *l, int index, pn_value_t v);
int pn_list_add(pn_list_t *l, pn_value_t v);
bool pn_list_remove(pn_list_t *l, pn_value_t v);
pn_value_t pn_list_pop(pn_list_t *l, int index);
int pn_list_extend(pn_list_t *l, const char *fmt, ...);
int pn_list_fill(pn_list_t *l, pn_value_t v, int n);
void pn_list_clear(pn_list_t *l);
int pn_list_size(pn_list_t *l);
size_t pn_encode_sizeof_list(pn_list_t *l);
size_t pn_encode_list(pn_list_t *l, char *out);

/* maps */

pn_map_t *pn_map(int capacity);
int pn_map_set(pn_map_t *map, pn_value_t key, pn_value_t value);
pn_value_t pn_map_get(pn_map_t *map, pn_value_t key);
pn_value_t pn_map_pop(pn_map_t *map, pn_value_t key);
size_t pn_encode_sizeof_map(pn_map_t *map);
size_t pn_encode_map(pn_map_t *m, char *out);

/* tags */

pn_tag_t *pn_tag(pn_value_t descriptor, pn_value_t value);
pn_value_t pn_tag_descriptor(pn_tag_t *t);
pn_value_t pn_tag_value(pn_tag_t *t);
size_t pn_encode_sizeof_tag(pn_tag_t *t);
size_t pn_encode_tag(pn_tag_t *t, char *out);

/* random */

int pn_fmt(char **pos, char *limit, const char *fmt, ...);

#endif /* value.h */
