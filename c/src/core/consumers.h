#ifndef PROTON_CONSUMERS_H
#define PROTON_CONSUMERS_H

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

/* Definitions of AMQP type codes */
#include "encodings.h"

#include "proton/codec.h"
#include "proton/types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct pni_consumer_t {
  const uint8_t* output_start;
  size_t size;
  size_t position;
} pni_consumer_t;

static inline pni_consumer_t make_consumer_from_bytes(pn_bytes_t output_bytes) {
  return (pni_consumer_t){
    .output_start = (const uint8_t*) output_bytes.start,
    .size = output_bytes.size,
    .position = 0
  };
}

static inline bool pni_consumer_readf8(pni_consumer_t *consumer, uint8_t* result)
{
  if (consumer->position+1 > consumer->size) {
    consumer->position = consumer->size;
    return false;
  }
  uint8_t r = consumer->output_start[consumer->position+0];
  consumer->position++;
  *result = r;
  return true;
}

static inline bool pni_consumer_readf16(pni_consumer_t *consumer, uint16_t* result)
{
  if (consumer->position+2 > consumer->size) {
    consumer->position = consumer->size;
    return false;
  }
  uint16_t a = consumer->output_start[consumer->position+0];
  uint16_t b = consumer->output_start[consumer->position+1];
  uint16_t r = a << 8
  | b;
  consumer->position += 2;
  *result = r;
  return true;
}

static inline bool pni_consumer_readf32(pni_consumer_t *consumer, uint32_t* result)
{
  if (consumer->position+4 > consumer->size) {
    consumer->position = consumer->size;
    return false;
  }
  uint32_t a = consumer->output_start[consumer->position+0];
  uint32_t b = consumer->output_start[consumer->position+1];
  uint32_t c = consumer->output_start[consumer->position+2];
  uint32_t d = consumer->output_start[consumer->position+3];
  uint32_t r = a << 24
  | b << 16
  | c <<  8
  | d;
  consumer->position += 4;
  *result = r;
  return true;
}

static inline bool pni_consumer_readf64(pni_consumer_t *consumer, uint64_t* result)
{
  uint32_t a;
  if (!pni_consumer_readf32(consumer, &a)) return false;
  uint32_t b;
  if (!pni_consumer_readf32(consumer, &b)) return false;
  *result = (uint64_t)a << 32 | (uint64_t)b;
  return true;
}

static inline bool pni_consumer_readf128(pni_consumer_t *consumer, void *dst)
{
  if (consumer->position+16 > consumer->size) {
    consumer->position = consumer->size;
    return false;
  }
  memcpy(dst, &consumer->output_start[consumer->position], 16);
  consumer->position += 16;
  return true;
}

static inline bool pni_consumer_readv8(pni_consumer_t *consumer, pn_bytes_t* bytes){
  uint8_t size;
  if (!pni_consumer_readf8(consumer, &size)) return false;
  if (consumer->position+size > consumer->size) {
    consumer->position = consumer->size;
    return false;
  }
  *bytes = (pn_bytes_t){.size=size,.start=(const char *)consumer->output_start+consumer->position};
  consumer->position += size;
  return true;
}

static inline bool pni_consumer_readv32(pni_consumer_t *consumer, pn_bytes_t* bytes){
  uint32_t size;
  if (!pni_consumer_readf32(consumer, &size)) return false;
  if (consumer->position+size > consumer->size) {
    consumer->position = consumer->size;
    return false;
  }
  *bytes = (pn_bytes_t){.size=size,.start=(const char *)consumer->output_start+consumer->position};
  consumer->position += size;
  return true;
}

static inline bool pni_consumer_read_value_not_described(pni_consumer_t* consumer, uint8_t type, pn_bytes_t *value) {
  uint8_t subcategory = type >> 4;
  switch (subcategory) {
    // Fixed width types:
    // No data
    case 0x4:
      *value = (pn_bytes_t){.size=0,.start=NULL};
      return true;
      // 1 Octet
    case 0x5:
      if (consumer->position+1 > consumer->size) break;
      *value = (pn_bytes_t){.size=1,.start=(const char *)consumer->output_start+consumer->position};
      consumer->position += 1;
      return true;
      // 2 Octets
    case 0x6:
      if (consumer->position+2 > consumer->size) break;
      *value = (pn_bytes_t){.size=2,.start=(const char *)consumer->output_start+consumer->position};
      consumer->position += 2;
      return true;
      // 4 Octets
    case 0x7:
      if (consumer->position+4 > consumer->size) break;
      *value = (pn_bytes_t){.size=4,.start=(const char *)consumer->output_start+consumer->position};
      consumer->position += 4;
      return true;
      // 8 Octets
    case 0x8:
      if (consumer->position+8 > consumer->size) break;
      *value = (pn_bytes_t){.size=8,.start=(const char *)consumer->output_start+consumer->position};
      consumer->position += 8;
      return true;
      // 16 Octets
    case 0x9:
      if (consumer->position+16 > consumer->size) break;
      *value = (pn_bytes_t){.size=16,.start=(const char *)consumer->output_start+consumer->position};
      consumer->position += 16;
      return true;
      // Variable width types:
      // One Octet of size
    case 0xA:
    case 0xC:
    case 0xE: {
      uint8_t size;
      if (!pni_consumer_readf8(consumer, &size)) return false;
      if (consumer->position+size > consumer->size) break;
      *value = (pn_bytes_t){.size=size,.start=(const char *)consumer->output_start+consumer->position};
      consumer->position += size;
      return true;
    }
    // 4 Octets of size
    case 0xB:
    case 0xD:
    case 0xF: {
      uint32_t size;
      if (!pni_consumer_readf32(consumer, &size)) return false;
      if (consumer->position+size > consumer->size) break;
      *value = (pn_bytes_t){.size=size,.start=(const char *)consumer->output_start+consumer->position};
      consumer->position += size;
      return true;
    }
    default:
      break;
  }
  consumer->position = consumer->size;
  return false;
}

static inline bool pni_consumer_skip_value_not_described(pni_consumer_t* consumer, uint8_t type) {
  pn_bytes_t value;
  return pni_consumer_read_value_not_described(consumer, type, &value);
}

static inline bool pni_consumer_skip_value(pni_consumer_t* consumer, uint8_t type) {
  // Check for described type
  if (type==PNE_DESCRIPTOR) {
    // Skip descriptor
    if (!pni_consumer_readf8(consumer, &type)) return false;
    if (!pni_consumer_skip_value_not_described(consumer, type)) return false;
    if (!pni_consumer_readf8(consumer, &type)) return false;
    return pni_consumer_skip_value_not_described(consumer, type);
  }
  return pni_consumer_skip_value_not_described(consumer, type);
}

static inline bool pni_islist(pni_consumer_t* consumer) {
  uint8_t t;
  if (!pni_consumer_readf8(consumer, &t)) return false;
  switch (t) {
    case PNE_LIST0:
    case PNE_LIST8:
    case PNE_LIST32:
      return true;
    default:
      return false;
  }
}

///////////////////////////////////////////////////////////////////////////////

static inline bool consume_single_value_not_described(pni_consumer_t* consumer, uint8_t* type) {
  uint8_t t;
  if (!pni_consumer_readf8(consumer, &t)) return false;
  if (!pni_consumer_skip_value_not_described(consumer, t)) return false;
  if (t==0) return false;
  *type = t;
  return true;
}

static inline bool consume_single_value(pni_consumer_t* consumer, uint8_t* type) {
  uint8_t t;
  if (!pni_consumer_readf8(consumer, &t)) return false;
  *type = t;
  if (t==PNE_DESCRIPTOR) {
    uint8_t dummy;
    // Descriptor
    bool dq = consume_single_value_not_described(consumer, &dummy);
    // Value
    bool vq = consume_single_value_not_described(consumer, &dummy);
    return dq && vq;
  } else {
    return pni_consumer_skip_value_not_described(consumer, t);
  }
}

static inline bool consume_raw(pni_consumer_t* consumer, pn_bytes_t* raw) {
  size_t start = consumer->position;
  uint8_t type;
  bool succeed = consume_single_value(consumer, &type);
  if (succeed && type!=PNE_NULL) {
    *raw = (pn_bytes_t){.size=consumer->position-start, .start=(const char*)consumer->output_start+start};
  } else {
    *raw = (pn_bytes_t) {0, NULL};
  }
  return succeed;
}

static inline bool consume_anything(pni_consumer_t* consumer) {
  uint8_t dummy;
  return consume_single_value(consumer, &dummy);
}

static inline bool consume_ulong(pni_consumer_t* consumer, uint64_t *ulong) {
  *ulong = 0;
  uint8_t type;
  if (!pni_consumer_readf8(consumer, &type)) return false;
  switch (type) {
    case PNE_SMALLULONG: {
      uint8_t ul;
      if (!pni_consumer_readf8(consumer, &ul)) return false;
      *ulong = ul;
      return true;
    }
    case PNE_ULONG: {
      uint64_t ul;
      if (!pni_consumer_readf64(consumer, &ul)) return false;
      *ulong = ul;
      return true;
    }
    case PNE_ULONG0: {
      *ulong = 0;
      return true;
    }
    default:
      pni_consumer_skip_value(consumer, type);
      return false;
  }
}

static inline bool consume_uint(pni_consumer_t* consumer, uint32_t *uint) {
  *uint = 0;
  uint8_t type;
  if (!pni_consumer_readf8(consumer, &type)) return false;
  switch (type) {
    case PNE_SMALLUINT: {
      uint8_t ui;
      if (!pni_consumer_readf8(consumer, &ui)) return false;
      *uint = ui;
      return true;
    }
    case PNE_UINT: {
      uint32_t ui;
      if (!pni_consumer_readf32(consumer, &ui)) return false;
      *uint = ui;
      return true;
    }
    case PNE_UINT0: {
      *uint = 0;
      return true;
    }
    default:
      pni_consumer_skip_value(consumer, type);
      return false;
  }
}

static inline bool consume_ushort(pni_consumer_t* consumer, uint16_t *ushort) {
  *ushort = 0;
  uint8_t type;
  if (!pni_consumer_readf8(consumer, &type)) return false;
  switch (type) {
    case PNE_USHORT: {
      uint16_t us;
      if (!pni_consumer_readf16(consumer, &us)) return false;
      *ushort = us;
      return true;
    }
    default:
      pni_consumer_skip_value(consumer, type);
      return false;
  }
}

static inline bool consume_ubyte(pni_consumer_t* consumer, uint8_t *ubyte) {
  *ubyte = 0;
  uint8_t type;
  if (!pni_consumer_readf8(consumer, &type)) return false;
  switch (type) {
    case PNE_UBYTE: {
      uint8_t ub;
      if (!pni_consumer_readf8(consumer, &ub)) return false;
      *ubyte = ub;
      return true;
    }
    default:
      pni_consumer_skip_value(consumer, type);
      return false;
  }
}

static inline bool consume_bool(pni_consumer_t* consumer, bool *b) {
  *b = false;
  uint8_t type;
  if (!pni_consumer_readf8(consumer, &type)) return false;
  switch (type) {
    case PNE_BOOLEAN: {
      uint8_t ub;
      if (!pni_consumer_readf8(consumer, &ub)) return false;
      *b = ub;
      return true;
    }
    case PNE_FALSE:
      *b = false;
      return true;
    case PNE_TRUE:
      *b = true;
      return true;
    default:
      pni_consumer_skip_value(consumer, type);
      return false;
  }
}

static inline bool consume_timestamp(pni_consumer_t* consumer, pn_timestamp_t *timestamp) {
  *timestamp = 0;
  uint8_t type;
  if (!pni_consumer_readf8(consumer, &type)) return false;
  switch (type) {
    case PNE_MS64: {
      return pni_consumer_readf64(consumer, (uint64_t*)timestamp);
    }
    default:
      return false;
  }
}

static inline bool consume_atom(pni_consumer_t* consumer, pn_atom_t *atom) {
  uint8_t type;
  if (pni_consumer_readf8(consumer, &type)) {
    switch (type) {
    case PNE_SMALLULONG: {
      uint8_t ul;
      if (!pni_consumer_readf8(consumer, &ul)) break;
      atom->type = PN_ULONG;
      atom->u.as_ulong = ul;
      return true;
    }
    case PNE_ULONG: {
      uint64_t ul;
      if (!pni_consumer_readf64(consumer, &ul)) break;
      atom->type = PN_ULONG;
      atom->u.as_ulong = ul;
      return true;
    }
    case PNE_ULONG0: {
      atom->type = PN_ULONG;
      atom->u.as_ulong = 0;
      return true;
    }
    case PNE_SMALLUINT: {
      uint8_t ui;
      if (!pni_consumer_readf8(consumer, &ui)) break;
      atom->type = PN_UINT;
      atom->u.as_uint = ui;
      return true;
    }
    case PNE_UINT: {
      uint32_t ui;
      if (!pni_consumer_readf32(consumer, &ui)) break;
      atom->type = PN_UINT;
      atom->u.as_uint = ui;
      return true;
    }
    case PNE_UINT0: {
      atom->type = PN_UINT;
      atom->u.as_uint = 0;
      return true;
    }
    case PNE_USHORT: {
      uint16_t us;
      if (!pni_consumer_readf16(consumer, &us)) break;
      atom->type = PN_USHORT;
      atom->u.as_ushort = us;
      return true;
    }
    case PNE_UBYTE: {
      uint8_t ub;
      if (!pni_consumer_readf8(consumer, &ub)) break;
      atom->type = PN_UBYTE;
      atom->u.as_ubyte = ub;
      return true;
    }
    case PNE_BOOLEAN: {
      uint8_t ub;
      if (!pni_consumer_readf8(consumer, &ub)) break;
      atom->type = PN_BOOL;
      atom->u.as_bool = ub;
      return true;
    }
    case PNE_FALSE:
      atom->type = PN_BOOL;
      atom->u.as_bool = false;
      return true;
    case PNE_TRUE:
      atom->type = PN_BOOL;
      atom->u.as_bool = true;
      return true;
    case PNE_MS64: {
      uint64_t timestamp;
      if (!pni_consumer_readf64(consumer, &timestamp)) break;
      atom->type = PN_TIMESTAMP;
      atom->u.as_timestamp = timestamp;
      return true;
    }
    case PNE_VBIN32:{
      pn_bytes_t binary;
      if (!pni_consumer_readv32(consumer, &binary)) break;
      atom->type = PN_BINARY;
      atom->u.as_bytes = binary;
      return true;
    }
    case PNE_VBIN8:{
      pn_bytes_t binary;
      if (!pni_consumer_readv8(consumer, &binary)) break;
      atom->type = PN_BINARY;
      atom->u.as_bytes = binary;
      return true;
    }
    case PNE_STR32_UTF8: {
      pn_bytes_t string;
      if (!pni_consumer_readv32(consumer, &string)) break;
      atom->type = PN_STRING;
      atom->u.as_bytes = string;
      return true;
    }
    case PNE_STR8_UTF8: {
      pn_bytes_t string;
      if (!pni_consumer_readv8(consumer, &string)) break;
      atom->type = PN_STRING;
      atom->u.as_bytes = string;
      return true;
    }
    case PNE_SYM32:{
      pn_bytes_t symbol;
      if (!pni_consumer_readv32(consumer, &symbol)) break;
      atom->type = PN_SYMBOL;
      atom->u.as_bytes = symbol;
      return true;
    }
    case PNE_SYM8:{
      pn_bytes_t symbol;
      if (!pni_consumer_readv8(consumer, &symbol)) break;
      atom->type = PN_SYMBOL;
      atom->u.as_bytes = symbol;
      return true;
    }
    case PNE_UUID: {
      pn_uuid_t uuid;
      if (!pni_consumer_readf128(consumer, &uuid)) break;
      atom->type = PN_UUID;
      atom->u.as_uuid = uuid;
      return true;
    }
    case PNE_NULL:
      atom->type = PN_NULL;
      return true;
    default:
      pni_consumer_skip_value(consumer, type);
      break;
    }
  }
  atom->type = PN_NULL;
  return false;
}

// XXX: assuming numeric -
// if we get a symbol we should map it to the numeric value and dispatch on that
static inline bool consume_described_ulong_descriptor(pni_consumer_t* consumer, pni_consumer_t *subconsumer, uint64_t *descriptor) {
  *descriptor = 0;
  *subconsumer = (pni_consumer_t){.output_start=NULL, .position=0, .size=0};
  uint8_t type;
  if (!pni_consumer_readf8(consumer, &type)) return false;
  switch (type) {
    case PNE_DESCRIPTOR: {
      bool dq = consume_ulong(consumer, descriptor);
      size_t sposition = consumer->position;
      uint8_t dummy;
      bool vq = consume_single_value(consumer, &dummy);
      if (dq && vq) {
        size_t scsize = consumer->position > sposition ? consumer->position-sposition : 0;
        *subconsumer = (pni_consumer_t){.output_start=consumer->output_start+sposition, .position=0, .size=scsize};
        return true;
      }
      return false;
    }
    default:
      pni_consumer_skip_value_not_described(consumer, type);
      return false;
  }
}

static inline bool consume_described(pni_consumer_t* consumer, pni_consumer_t *subconsumer) {
  *subconsumer = (pni_consumer_t){.output_start=NULL, .position=0, .size=0};
  uint8_t type;
  if (!pni_consumer_readf8(consumer, &type)) return false;
  switch (type) {
    case PNE_DESCRIPTOR: {
      uint8_t dummy;
      bool dq = consume_single_value_not_described(consumer, &dummy);
      size_t sposition = consumer->position;
      bool vq = consume_single_value(consumer, &dummy);
      if (dq && vq) {
        size_t scsize = consumer->position > sposition ? consumer->position-sposition : 0;
        *subconsumer = (pni_consumer_t){.output_start=consumer->output_start+sposition, .position=0, .size=scsize};
        return true;
      }
      return false;
    }
    default:
      pni_consumer_skip_value_not_described(consumer, type);
      return false;
  }
}

static inline bool consume_list(pni_consumer_t* consumer, pni_consumer_t *subconsumer, uint32_t *count) {
  *subconsumer = (pni_consumer_t){.output_start=NULL, .position=0, .size=0};
  *count = 0;
  uint8_t type;
  if (!pni_consumer_readf8(consumer, &type)) return false;
  switch (type) {
    case PNE_LIST32: {
      uint32_t s;
      if (!pni_consumer_readf32(consumer, &s)) return false;
      size_t scsize = s < consumer->size-consumer->position ? s : consumer->size-consumer->position;
      *subconsumer = (pni_consumer_t){.output_start=consumer->output_start+consumer->position, .position=0, .size=scsize};
      consumer->position += scsize;
      return pni_consumer_readf32(subconsumer, count);
    }
    case PNE_LIST8: {
      uint8_t s;
      if (!pni_consumer_readf8(consumer, &s)) return false;
      size_t scsize = s < consumer->size-consumer->position ? s : consumer->size-consumer->position;
      *subconsumer = (pni_consumer_t){.output_start=consumer->output_start+consumer->position, .position=0, .size=scsize};
      consumer->position += scsize;
      uint8_t c;
      if (!pni_consumer_readf8(subconsumer, &c)) return false;
      *count = c;
      return true;
    }
    case PNE_LIST0:
      return true;
    default:
      pni_consumer_skip_value(consumer, type);
      return false;
  }
}

// TODO: This is currently a placeholder - maybe not actually needed
static inline bool consume_end_list(pni_consumer_t *consumer) {
  return true;
}

static inline bool consume_array(pni_consumer_t* consumer, pni_consumer_t *subconsumer, uint32_t *count, uint8_t *element_type) {
  *subconsumer = (pni_consumer_t){.output_start=NULL, .position=0, .size=0};
  *count = 0;
  *element_type = 0;
  uint8_t type;
  if (!pni_consumer_readf8(consumer, &type)) return false;
  switch (type) {
    case PNE_ARRAY32: {
      uint32_t s;
      if (!pni_consumer_readf32(consumer, &s)) return false;
      size_t scsize = s < consumer->size-consumer->position ? s : consumer->size-consumer->position;
      *subconsumer = (pni_consumer_t){.output_start=consumer->output_start+consumer->position, .position=0, .size=scsize};
      consumer->position += scsize;
      if (!pni_consumer_readf32(subconsumer, count)) return false;
      return pni_consumer_readf8(subconsumer, element_type);
    }
    case PNE_ARRAY8: {
      uint8_t s;
      if (!pni_consumer_readf8(consumer, &s)) return false;
      size_t scsize = s < consumer->size-consumer->position ? s : consumer->size-consumer->position;
      *subconsumer = (pni_consumer_t){.output_start=consumer->output_start+consumer->position, .position=0, .size=scsize};
      consumer->position += scsize;
      uint8_t c;
      if (!pni_consumer_readf8(subconsumer, &c)) return false;
      *count = c;
      return pni_consumer_readf8(subconsumer, element_type);
    }
    default:
      pni_consumer_skip_value(consumer, type);
      return false;
  }
}

static inline bool consume_described_anything(pni_consumer_t* consumer) {
  uint8_t type;
  bool tq = consume_single_value(consumer, &type);
  return tq && type==PNE_DESCRIPTOR;
}

static inline bool consume_described_type_anything(pni_consumer_t* consumer, uint64_t *type) {
  pni_consumer_t subconsumer;
  return consume_described_ulong_descriptor(consumer, &subconsumer, type);
}

static inline bool consume_described_maybe_type_anything(pni_consumer_t* consumer, bool *qtype, uint64_t *type) {
  pni_consumer_t subconsumer;
  *qtype = consume_described_ulong_descriptor(consumer, &subconsumer, type);
  return *qtype;
}

static inline bool consume_described_maybe_type_raw(pni_consumer_t *consumer, bool *qtype, uint64_t *type, pn_bytes_t *raw) {
  pni_consumer_t subconsumer;
  *qtype = consume_described_ulong_descriptor(consumer, &subconsumer, type);
  return *qtype && consume_raw(&subconsumer, raw);
}

static inline bool consume_described_maybe_type_maybe_anything(pni_consumer_t *consumer, bool *qtype, uint64_t *type, bool *qanything) {
  pni_consumer_t subconsumer;
  *qtype = consume_described_ulong_descriptor(consumer, &subconsumer, type);
  *qanything = consume_anything(&subconsumer);
  return *qtype && *qanything;
}

static inline bool consume_described_raw(pni_consumer_t *consumer, pn_bytes_t *raw) {
  pni_consumer_t subconsumer;
  return consume_described(consumer, &subconsumer) && consume_raw(&subconsumer, raw);
}

static inline bool consume_string(pni_consumer_t *consumer, pn_bytes_t *string) {
  uint8_t type;
  *string = (pn_bytes_t){.size=0, .start=0};
  if (!pni_consumer_readf8(consumer, &type)) return false;
  switch (type) {
    case PNE_STR32_UTF8: {
      return pni_consumer_readv32(consumer, string);
    }
    case PNE_STR8_UTF8: {
      return pni_consumer_readv8(consumer, string);
    }
    default:
      pni_consumer_skip_value(consumer, type);
      return false;
  }
}

static inline bool consume_symbol(pni_consumer_t *consumer, pn_bytes_t *symbol) {
  uint8_t type;
  *symbol = (pn_bytes_t){.size=0, .start=0};
  if (!pni_consumer_readf8(consumer, &type)) return false;
  switch (type) {
    case PNE_SYM32:{
      return pni_consumer_readv32(consumer, symbol);
    }
    case PNE_SYM8:{
      return pni_consumer_readv8(consumer, symbol);
    }
    default:
      pni_consumer_skip_value(consumer, type);
      return false;
  }
}

static inline bool consume_binaryornull(pni_consumer_t *consumer, pn_bytes_t *binary) {
  uint8_t type;
  *binary  = (pn_bytes_t){.size=0, .start=0};
  if (!pni_consumer_readf8(consumer, &type)) return false;
  switch (type) {
    case PNE_NULL:{
      return true;
    }
    case PNE_VBIN32:{
      return pni_consumer_readv32(consumer, binary);
    }
    case PNE_VBIN8:{
      return pni_consumer_readv8(consumer, binary);
    }
    default:
      pni_consumer_skip_value(consumer, type);
      return false;
  }
}

#endif // PROTON_CONSUMERS_H
