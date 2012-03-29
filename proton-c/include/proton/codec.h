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
#include <wchar.h>

int pn_write_descriptor(char **pos, char *limit);
int pn_write_null(char **pos, char *limit);
int pn_write_boolean(char **pos, char *limit, bool v);
int pn_write_ubyte(char **pos, char *limit, uint8_t v);
int pn_write_byte(char **pos, char *limit, int8_t v);
int pn_write_ushort(char **pos, char *limit, uint16_t v);
int pn_write_short(char **pos, char *limit, int16_t v);
int pn_write_uint(char **pos, char *limit, uint32_t v);
int pn_write_int(char **pos, char *limit, int32_t v);
int pn_write_char(char **pos, char *limit, wchar_t v);
int pn_write_float(char **pos, char *limit, float v);
int pn_write_ulong(char **pos, char *limit, uint64_t v);
int pn_write_long(char **pos, char *limit, int64_t v);
int pn_write_double(char **pos, char *limit, double v);
int pn_write_binary(char **pos, char *limit, size_t size, const char *src);
int pn_write_utf8(char **pos, char *limit, size_t size, char *utf8);
int pn_write_symbol(char **pos, char *limit, size_t size, const char *symbol);
int pn_write_start(char **pos, char *limit, char **start);
int pn_write_list(char **pos, char *limit, char *start, size_t count);
int pn_write_map(char **pos, char *limit, char *start, size_t count);

typedef struct {
  void (*on_null)(void *ctx);
  void (*on_bool)(void *ctx, bool v);
  void (*on_ubyte)(void *ctx, uint8_t v);
  void (*on_byte)(void *ctx, int8_t v);
  void (*on_ushort)(void *ctx, uint16_t v);
  void (*on_short)(void *ctx, int16_t v);
  void (*on_uint)(void *ctx, uint32_t v);
  void (*on_int)(void *ctx, int32_t v);
  void (*on_float)(void *ctx, float v);
  void (*on_ulong)(void *ctx, uint64_t v);
  void (*on_long)(void *ctx, int64_t v);
  void (*on_double)(void *ctx, double v);
  void (*on_binary)(void *ctx, size_t size, char *bytes);
  void (*on_utf8)(void *ctx, size_t size, char *utf8);
  void (*on_symbol)(void *ctx, size_t size, char *str);
  void (*start_descriptor)(void *ctx);
  void (*stop_descriptor)(void *ctx);
  void (*start_array)(void *ctx, size_t count, uint8_t code);
  void (*stop_array)(void *ctx, size_t count, uint8_t code);
  void (*start_list)(void *ctx, size_t count);
  void (*stop_list)(void *ctx, size_t count);
  void (*start_map)(void *ctx, size_t count);
  void (*stop_map)(void *ctx, size_t count);
} pn_data_callbacks_t;

ssize_t pn_read_datum(const char *bytes, size_t n, pn_data_callbacks_t *cb, void *ctx);

#define PN_DATA_CALLBACKS(STEM) ((pn_data_callbacks_t) { \
  .on_null = & STEM ## _null,                              \
  .on_bool = & STEM ## _bool,                              \
  .on_ubyte = & STEM ## _ubyte,                            \
  .on_byte = & STEM ## _byte,                              \
  .on_ushort = & STEM ## _ushort,                          \
  .on_short = & STEM ## _short,                            \
  .on_uint = & STEM ## _uint,                              \
  .on_int = & STEM ## _int,                                \
  .on_float = & STEM ## _float,                            \
  .on_ulong = & STEM ## _ulong,                            \
  .on_long = & STEM ## _long,                              \
  .on_double = & STEM ## _double,                          \
  .on_binary = & STEM ## _binary,                          \
  .on_utf8 = & STEM ## _utf8,                              \
  .on_symbol = & STEM ## _symbol,                          \
  .start_descriptor = & STEM ## _start_descriptor,         \
  .stop_descriptor = & STEM ## _stop_descriptor,           \
  .start_array = & STEM ## _start_array,                   \
  .stop_array = & STEM ## _stop_array,                     \
  .start_list = & STEM ## _start_list,                     \
  .stop_list = & STEM ## _stop_list,                       \
  .start_map = & STEM ## _start_map,                       \
  .stop_map = & STEM ## _stop_map,                         \
})

extern pn_data_callbacks_t *noop;
extern pn_data_callbacks_t *printer;

#endif /* codec.h */
