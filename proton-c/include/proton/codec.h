#ifndef PROTON_CODEC_H
#define PROTON_CODEC_H 1

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

#include <proton/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  PN_NULL,
  PN_BOOL,
  PN_UBYTE,
  PN_BYTE,
  PN_USHORT,
  PN_SHORT,
  PN_UINT,
  PN_INT,
  PN_ULONG,
  PN_LONG,
  PN_FLOAT,
  PN_DOUBLE,
  PN_BINARY,
  PN_STRING,
  PN_SYMBOL,
  PN_DESCRIPTOR,
  PN_ARRAY,
  PN_LIST,
  PN_MAP,
  PN_TYPE
} pn_type_t;

typedef struct {
  pn_type_t type;
  union {
    bool as_bool;
    uint8_t as_ubyte;
    int8_t as_byte;
    uint16_t as_ushort;
    int16_t as_short;
    uint32_t as_uint;
    int32_t as_int;
    uint64_t as_ulong;
    int64_t as_long;
    float as_float;
    double as_double;
    pn_bytes_t as_binary;
    pn_bytes_t as_string;
    pn_bytes_t as_symbol;
    size_t count;
    pn_type_t type;
  } u;
} pn_atom_t;

// data

typedef struct pn_data_t pn_data_t;

pn_data_t *pn_data(size_t capacity);
void pn_data_free(pn_data_t *data);
int pn_data_errno(pn_data_t *data);
const char *pn_data_error(pn_data_t *data);
int pn_data_vfill(pn_data_t *data, const char *fmt, va_list ap);
int pn_data_fill(pn_data_t *data, const char *fmt, ...);
int pn_data_vscan(pn_data_t *data, const char *fmt, va_list ap);
int pn_data_scan(pn_data_t *data, const char *fmt, ...);

int pn_data_clear(pn_data_t *data);
size_t pn_data_size(pn_data_t *data);
void pn_data_rewind(pn_data_t *data);
bool pn_data_next(pn_data_t *data, pn_type_t *type);
bool pn_data_prev(pn_data_t *data, pn_type_t *type);
bool pn_data_enter(pn_data_t *data);
bool pn_data_exit(pn_data_t *data);

int pn_data_print(pn_data_t *data);
int pn_data_format(pn_data_t *data, char *bytes, size_t *size);
ssize_t pn_data_encode(pn_data_t *data, char *bytes, size_t size);
ssize_t pn_data_decode(pn_data_t *data, char *bytes, size_t size);

int pn_data_put_list(pn_data_t *data);
int pn_data_put_map(pn_data_t *data);
int pn_data_put_array(pn_data_t *data, bool described, pn_type_t type);
int pn_data_put_described(pn_data_t *data);
int pn_data_put_null(pn_data_t *data);
int pn_data_put_bool(pn_data_t *data, bool b);
int pn_data_put_ubyte(pn_data_t *data, uint8_t ub);
int pn_data_put_byte(pn_data_t *data, int8_t b);
int pn_data_put_ushort(pn_data_t *data, uint16_t us);
int pn_data_put_short(pn_data_t *data, int16_t s);
int pn_data_put_uint(pn_data_t *data, uint32_t ui);
int pn_data_put_int(pn_data_t *data, int32_t i);
int pn_data_put_ulong(pn_data_t *data, uint64_t ul);
int pn_data_put_long(pn_data_t *data, int64_t l);
int pn_data_put_float(pn_data_t *data, float f);
int pn_data_put_double(pn_data_t *data, double d);
int pn_data_put_binary(pn_data_t *data, pn_bytes_t bytes);
int pn_data_put_string(pn_data_t *data, pn_bytes_t string);
int pn_data_put_symbol(pn_data_t *data, pn_bytes_t symbol);

int pn_data_get_list(pn_data_t *data, size_t *count);
int pn_data_get_map(pn_data_t *data, size_t *count);
int pn_data_get_array(pn_data_t *data, size_t *count, bool *described, pn_type_t *type);
int pn_data_get_described(pn_data_t *data);
int pn_data_get_null(pn_data_t *data);
int pn_data_get_bool(pn_data_t *data, bool *b);
int pn_data_get_ubyte(pn_data_t *data, uint8_t *ub);
int pn_data_get_byte(pn_data_t *data, int8_t *b);
int pn_data_get_ushort(pn_data_t *data, uint16_t *us);
int pn_data_get_short(pn_data_t *data, int16_t *s);
int pn_data_get_uint(pn_data_t *data, uint32_t *ui);
int pn_data_get_int(pn_data_t *data, int32_t *i);
int pn_data_get_ulong(pn_data_t *data, uint64_t *ul);
int pn_data_get_long(pn_data_t *data, int64_t *l);
int pn_data_get_float(pn_data_t *data, float *f);
int pn_data_get_double(pn_data_t *data, double *d);
int pn_data_get_binary(pn_data_t *data, pn_bytes_t *bytes);
int pn_data_get_string(pn_data_t *data, pn_bytes_t *string);
int pn_data_get_symbol(pn_data_t *data, pn_bytes_t *symbol);

void pn_data_dump(pn_data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* codec.h */
