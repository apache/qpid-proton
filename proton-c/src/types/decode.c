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
#include <iconv.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "encodings.h"
#include "value-internal.h"

static enum TYPE amqp_code_to_type(uint8_t code)
{
  switch (code)
  {
  case PNE_NULL: return EMPTY;
    //  case PNE_TRUE:
    //  case PNE_FALSE:
    //  case PNE_BOOLEAN: return BOOLEAN;
  case PNE_UBYTE: return UBYTE;
  case PNE_BYTE: return BYTE;
  case PNE_USHORT: return USHORT;
  case PNE_SHORT: return SHORT;
  case PNE_UINT:
  case PNE_UINT0: return UINT;
  case PNE_INT: return INT;
  case PNE_FLOAT: return FLOAT;
  case PNE_ULONG0:
  case PNE_ULONG: return ULONG;
  case PNE_LONG: return LONG;
  case PNE_DOUBLE: return DOUBLE;
  case PNE_VBIN8:
  case PNE_VBIN32: return BINARY;
  case PNE_STR8_UTF8:
  case PNE_STR32_UTF8: return STRING;
  case PNE_SYM8:
  case PNE_SYM32: return SYMBOL;
  case PNE_LIST0:
  case PNE_LIST8:
  case PNE_LIST32: return LIST;
  case PNE_ARRAY8:
  case PNE_ARRAY32: return ARRAY;
  case PNE_MAP8:
  case PNE_MAP32: return MAP;
  }
  return -1;
}

struct pn_decode_context_frame_st {
  size_t count;
  size_t limit;
  pn_value_t *values;
};

struct pn_decode_context_st {
  size_t depth;
  struct pn_decode_context_frame_st frames[1024];
};

#define CTX_CAST(ctx) ((struct pn_decode_context_st *) (ctx))

static void push_frame(void *ptr, pn_value_t *values, size_t limit)
{
  struct pn_decode_context_st *ctx = CTX_CAST(ptr);
  struct pn_decode_context_frame_st *frm = &ctx->frames[ctx->depth++];
  frm->count = 0;
  frm->limit = limit;
  frm->values = values;
}

static struct pn_decode_context_frame_st *frame(void *ptr)
{
  struct pn_decode_context_st *ctx = CTX_CAST(ptr);
  return &ctx->frames[ctx->depth-1];
}

static void autopop(void *ptr)
{
  struct pn_decode_context_st *ctx = CTX_CAST(ptr);
  struct pn_decode_context_frame_st *frm = frame(ptr);
  while (frm->limit && frm->count == frm->limit) {
    ctx->depth--;
    frm = frame(ptr);
  }
}

static void pop_frame(void *ptr)
{
  autopop(ptr);
  struct pn_decode_context_st *ctx = CTX_CAST(ptr);
  ctx->depth--;
}

static pn_value_t *next_value(void *ptr)
{
  autopop(ptr);
  struct pn_decode_context_frame_st *frm = frame(ptr);
  pn_value_t *result = &frm->values[frm->count++];
  return result;
}

static pn_value_t *curr_value(void *ptr)
{
  struct pn_decode_context_st *ctx = CTX_CAST(ptr);
  struct pn_decode_context_frame_st *frm = &ctx->frames[ctx->depth-1];
  return &frm->values[frm->count - 1];
}

void pn_decode_null(void *ctx) {
  pn_value_t *value = next_value(ctx);
  value->type = EMPTY;
}
void pn_decode_bool(void *ctx, bool v) {
  pn_value_t *value = next_value(ctx);
  value->type = BOOLEAN;
  value->u.as_boolean = v;
}
void pn_decode_ubyte(void *ctx, uint8_t v) {
  pn_value_t *value = next_value(ctx);
  value->type = UBYTE;
  value->u.as_ubyte = v;
}
void pn_decode_byte(void *ctx, int8_t v) {
  pn_value_t *value = next_value(ctx);
  value->type = BYTE;
  value->u.as_byte = v;
}
void pn_decode_ushort(void *ctx, uint16_t v) {
  pn_value_t *value = next_value(ctx);
  value->type = USHORT;
  value->u.as_ushort = v;
}
void pn_decode_short(void *ctx, int16_t v) {
  pn_value_t *value = next_value(ctx);
  value->type = SHORT;
  value->u.as_short = v;
}
void pn_decode_uint(void *ctx, uint32_t v) {
  pn_value_t *value = next_value(ctx);
  value->type = UINT;
  value->u.as_uint = v;
}
void pn_decode_int(void *ctx, int32_t v) {
  pn_value_t *value = next_value(ctx);
  value->type = INT;
  value->u.as_int = v;
}
void pn_decode_float(void *ctx, float f) {
  pn_value_t *value = next_value(ctx);
  value->type = FLOAT;
  value->u.as_float = f;
}
void pn_decode_ulong(void *ctx, uint64_t v) {
  pn_value_t *value = next_value(ctx);
  value->type = ULONG;
  value->u.as_ulong = v;
}
void pn_decode_long(void *ctx, int64_t v) {
  pn_value_t *value = next_value(ctx);
  value->type = LONG;
  value->u.as_long = v;
}
void pn_decode_double(void *ctx, double v) {
  pn_value_t *value = next_value(ctx);
  value->type = DOUBLE;
  value->u.as_double = v;
}
void pn_decode_binary(void *ctx, size_t size, char *bytes) {
  pn_value_t *value = next_value(ctx);
  value->type = BINARY;
  value->u.as_binary = pn_binary(bytes, size);
}
void pn_decode_utf8(void *ctx, size_t size, char *bytes) {
  pn_value_t *value = next_value(ctx);
  value->type = STRING;
  size_t remaining = (size+1)*sizeof(wchar_t);
  wchar_t buf[size+1];
  iconv_t cd = iconv_open("WCHAR_T", "UTF-8");
  wchar_t *out = buf;
  size_t n = iconv(cd, &bytes, &size, (char **)&out, &remaining);
  if (n == -1)
  {
    perror("pn_decode_utf8");
  }
  *out = L'\0';
  iconv_close(cd);
  value->u.as_string = pn_string(buf);
}
void pn_decode_symbol(void *ctx, size_t size, char *bytes) {
  pn_value_t *value = next_value(ctx);
  value->type = SYMBOL;
  value->u.as_symbol = pn_symboln(bytes, size);
}

void pn_decode_start_array(void *ctx, size_t count, uint8_t code) {
  pn_value_t *value = next_value(ctx);
  value->type = ARRAY;
  value->u.as_array = pn_array(amqp_code_to_type(code), count);
  push_frame(ctx, value->u.as_array->values, 0);
}
void pn_decode_stop_array(void *ctx, size_t count, uint8_t code) {
  pop_frame(ctx);
  pn_value_t *value = curr_value(ctx);
  value->u.as_array->size = count;
}

void pn_decode_start_list(void *ctx, size_t count) {
  pn_value_t *value = next_value(ctx);
  value->type = LIST;
  value->u.as_list = pn_list(count);
  push_frame(ctx, value->u.as_list->values, 0);
}

void pn_decode_stop_list(void *ctx, size_t count) {
  pop_frame(ctx);
  pn_value_t *value = curr_value(ctx);
  value->u.as_list->size = count;
}

void pn_decode_start_map(void *ctx, size_t count) {
  pn_value_t *value = next_value(ctx);
  value->type = MAP;
  value->u.as_map = pn_map(count/2);
  push_frame(ctx, value->u.as_map->pairs, 0);
}

void pn_decode_stop_map(void *ctx, size_t count) {
  pop_frame(ctx);
  pn_value_t *value = curr_value(ctx);
  value->u.as_map->size = count/2;
}

void pn_decode_start_descriptor(void *ctx) {
  pn_value_t *value = next_value(ctx);
  value->type = TAG;
  value->u.as_tag = pn_tag(EMPTY_VALUE, EMPTY_VALUE);
  push_frame(ctx, &value->u.as_tag->descriptor, 0);
}

void pn_decode_stop_descriptor(void *ctx) {
  pop_frame(ctx);
  pn_value_t *value = curr_value(ctx);
  push_frame(ctx, &value->u.as_tag->value, 1);
}

pn_data_callbacks_t *pn_decoder = &PN_DATA_CALLBACKS(pn_decode);

ssize_t pn_decode(pn_value_t *v, char *bytes, size_t n)
{
  struct pn_decode_context_st ctx = {.depth = 0};
  push_frame(&ctx, v, 0);
  ssize_t read = pn_read_datum(bytes, n, pn_decoder, &ctx);
  return read;
}
