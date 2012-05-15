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

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <proton/errors.h>
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
  size_t size;
  char *start;
} pn_bytes_t;

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
} pn_datum_t;

typedef struct {
  size_t size;
  pn_datum_t *start;
} pn_data_t;

// XXX: incremental decode and scan/fill both ways could be used for things like accessing lists/arrays
int pn_decode_data(pn_bytes_t *bytes, pn_data_t *data);
int pn_encode_data(pn_bytes_t *bytes, pn_data_t *data);
int pn_decode_one(pn_bytes_t *bytes, pn_data_t *data);

void pn_print_datum(pn_datum_t datum);
const char *pn_type_str(pn_type_t type);
int pn_pprint_data(const pn_data_t *data);
ssize_t pn_format_data(char *buf, size_t n, pn_data_t data);

int pn_fill_data(pn_data_t *data, const char *fmt, ...);
int pn_vfill_data(pn_data_t *data, const char *fmt, va_list ap);
int pn_scan_data(const pn_data_t *data, const char *fmt, ...);
int pn_vscan_data(const pn_data_t *data, const char *fmt, va_list ap);

// pn_bytes_t

pn_bytes_t pn_bytes(size_t size, char *start);
pn_bytes_t pn_bytes_dup(size_t size, const char *start);

#endif /* codec.h */
