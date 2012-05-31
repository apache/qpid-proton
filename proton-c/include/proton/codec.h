#ifndef _PROTON_CODEC_H
#define _PROTON_CODEC_H 1

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

typedef struct {
  size_t size;
  pn_atom_t *start;
} pn_atoms_t;

// XXX: incremental decode and scan/fill both ways could be used for things like accessing lists/arrays
int pn_decode_atoms(pn_bytes_t *bytes, pn_atoms_t *atoms);
int pn_encode_atoms(pn_bytes_t *bytes, pn_atoms_t *atoms);
int pn_decode_one(pn_bytes_t *bytes, pn_atoms_t *atoms);

int pn_print_atom(pn_atom_t atom);
const char *pn_type_str(pn_type_t type);
int pn_print_atoms(const pn_atoms_t *atoms);
ssize_t pn_format_atoms(char *buf, size_t n, pn_atoms_t atoms);
int pn_format_atom(pn_bytes_t *bytes, pn_atom_t atom);

int pn_fill_atoms(pn_atoms_t *atoms, const char *fmt, ...);
int pn_vfill_atoms(pn_atoms_t *atoms, const char *fmt, va_list ap);
int pn_scan_atoms(const pn_atoms_t *atoms, const char *fmt, ...);
int pn_vscan_atoms(const pn_atoms_t *atoms, const char *fmt, va_list ap);

// JSON

typedef struct pn_json_t pn_json_t;

pn_json_t *pn_json();
int pn_json_parse(pn_json_t *json, const char *str, pn_atoms_t *atoms);
int pn_json_render(pn_json_t *json, pn_atoms_t *atoms, char *output, size_t *size);
int pn_json_error_code(pn_json_t *json);
const char *pn_json_error_str(pn_json_t *json);
void pn_json_free(pn_json_t *json);

// data

typedef struct pn_data_t pn_data_t;

pn_data_t *pn_data(size_t capacity);
void pn_data_free(pn_data_t *data);
int pn_data_clear(pn_data_t *data);
int pn_data_decode(pn_data_t *data, char *bytes, size_t *size);
int pn_data_encode(pn_data_t *data, char *bytes, size_t *size);
int pn_data_vfill(pn_data_t *data, const char *fmt, va_list ap);
int pn_data_fill(pn_data_t *data, const char *fmt, ...);
int pn_data_vscan(pn_data_t *data, const char *fmt, va_list ap);
int pn_data_scan(pn_data_t *data, const char *fmt, ...);
int pn_data_print(pn_data_t *data);
int pn_data_format(pn_data_t *data, char *bytes, size_t *size);

#endif /* codec.h */
