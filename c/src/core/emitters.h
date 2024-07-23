#ifndef PROTON_EMITTERS_H
#define PROTON_EMITTERS_H

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
#include "engine-internal.h"
#include "protocol.h"
#include "util.h"

#include <proton/codec.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct current {
  struct current* previous_compound;
  size_t size_position;
  size_t start_position;
  size_t count;
  uint32_t null_count;
  uint8_t type;
  bool encoding_succeeded;
  bool is_described_list;
} pni_compound_context;

static inline pni_compound_context make_compound(void) {
  return (pni_compound_context){
    .count = 0
  };
}

typedef struct pni_emitter_t {
  char* output_start;
  size_t size;
  size_t position;
} pni_emitter_t;

static inline pni_emitter_t make_emitter_from_rwbytes(pn_rwbytes_t* output_bytes) {
  return (pni_emitter_t){
    .output_start = output_bytes->start,
    .size = output_bytes->size,
    .position = 0
  };
}

static inline pni_emitter_t make_emitter_from_bytes(pn_rwbytes_t output_bytes) {
  return (pni_emitter_t){
    .output_start = output_bytes.start,
    .size = output_bytes.size,
    .position = 0
  };
}

static inline pn_bytes_t make_bytes_from_emitter(pni_emitter_t emitter) {
    return (pn_bytes_t){.size = emitter.position, .start = emitter.output_start};
}

static inline bool resize_required(pni_emitter_t* emitter) {
  return emitter->position > emitter->size;
}

static inline void size_buffer_to_emitter(pn_rwbytes_t* buffer, pni_emitter_t* emitter) {
  pn_rwbytes_realloc(buffer, buffer->size+emitter->position-emitter->size);
}

static inline bool encode_succeeded(pni_emitter_t* emitter, pni_compound_context* compound) {
  return compound->encoding_succeeded;
}

static inline bool pni_emitter_remaining(pni_emitter_t* e, size_t need) {
  return (e->size >= e->position+need);
}

static inline void pni_emitter_writef8(pni_emitter_t* emitter, uint8_t value)
{
  if (pni_emitter_remaining(emitter, 1)) {
    emitter->output_start[emitter->position+0] = value;
  }
  emitter->position++;
}

static inline void pni_emitter_writef16(pni_emitter_t* emitter, uint16_t value)
{
  if (pni_emitter_remaining(emitter, 2)) {
    emitter->output_start[emitter->position+0] = 0xFF & (value >> 8);
    emitter->output_start[emitter->position+1] = 0xFF & (value     );
  }
  emitter->position += 2;
}

static inline void pni_emitter_writef32(pni_emitter_t* emitter, uint32_t value)
{
  if (pni_emitter_remaining(emitter, 4)) {
    emitter->output_start[emitter->position+0] = 0xFF & (value >> 24);
    emitter->output_start[emitter->position+1] = 0xFF & (value >> 16);
    emitter->output_start[emitter->position+2] = 0xFF & (value >>  8);
    emitter->output_start[emitter->position+3] = 0xFF & (value      );
  }
  emitter->position += 4;
}

static inline void pni_emitter_writef64(pni_emitter_t* emitter, uint64_t value) {
  if (pni_emitter_remaining(emitter, 8)) {
    emitter->output_start[emitter->position+0] = 0xFF & (value >> 56);
    emitter->output_start[emitter->position+1] = 0xFF & (value >> 48);
    emitter->output_start[emitter->position+2] = 0xFF & (value >> 40);
    emitter->output_start[emitter->position+3] = 0xFF & (value >> 32);
    emitter->output_start[emitter->position+4] = 0xFF & (value >> 24);
    emitter->output_start[emitter->position+5] = 0xFF & (value >> 16);
    emitter->output_start[emitter->position+6] = 0xFF & (value >>  8);
    emitter->output_start[emitter->position+7] = 0xFF & (value      );
  }
  emitter->position += 8;
}

static inline void pni_emitter_writef128(pni_emitter_t* emitter, void *value) {
  if (pni_emitter_remaining(emitter, 16)) {
    memcpy(emitter->output_start+emitter->position, value, 16);
  }
  emitter->position += 16;
}

static inline void pni_emitter_writev8(pni_emitter_t* emitter, const pn_bytes_t value)
{
  pni_emitter_writef8(emitter, value.size);
  if (pni_emitter_remaining(emitter, value.size))
    memcpy(emitter->output_start+emitter->position, value.start, value.size);
  emitter->position += value.size;
}

static inline void pni_emitter_writev32(pni_emitter_t* emitter, const pn_bytes_t value)
{
  pni_emitter_writef32(emitter, value.size);
  if (pni_emitter_remaining(emitter, value.size))
    memcpy(emitter->output_start+emitter->position, value.start, value.size);
  emitter->position += value.size;
}

static inline void pni_emitter_raw(pni_emitter_t* emitter, const pn_bytes_t raw)
{
  if (pni_emitter_remaining(emitter, raw.size))
    memcpy(emitter->output_start+emitter->position, raw.start, raw.size);
  emitter->position += raw.size;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static inline void emit_null(pni_emitter_t* emitter, pni_compound_context* compound) {
  if (compound->is_described_list) {
    compound->null_count++;
    return;
  }
  pni_emitter_writef8(emitter, PNE_NULL);
  compound->count++;
}

static inline void emit_accumulated_nulls(pni_emitter_t* emitter, pni_compound_context* compound) {
  for (uint32_t i=compound->null_count; i>0; --i) {
    pni_emitter_writef8(emitter, PNE_NULL);
    compound->count++;
  }
  compound->null_count = 0;
}

static inline void emit_bool(pni_emitter_t* emitter, pni_compound_context* compound, bool b) {
  emit_accumulated_nulls(emitter, compound);
  if (b) {
    pni_emitter_writef8(emitter, PNE_TRUE);
  } else {
    pni_emitter_writef8(emitter, PNE_FALSE);
  }
  compound->count++;
}

static inline void emit_ubyte(pni_emitter_t* emitter, pni_compound_context* compound, uint8_t ubyte) {
  emit_accumulated_nulls(emitter, compound);
  pni_emitter_writef8(emitter, PNE_UBYTE);
  pni_emitter_writef8(emitter, ubyte);
  compound->count++;
}

static inline void emit_ushort(pni_emitter_t* emitter, pni_compound_context* compound, uint16_t ushort) {
  emit_accumulated_nulls(emitter, compound);
  pni_emitter_writef8(emitter, PNE_USHORT);
  pni_emitter_writef16(emitter, ushort);
  compound->count++;
}

static inline void emit_uint(pni_emitter_t* emitter, pni_compound_context* compound, uint32_t uint) {
  emit_accumulated_nulls(emitter, compound);
  if (uint == 0) {
    pni_emitter_writef8(emitter, PNE_UINT0);
  } else if (uint < 256) {
    pni_emitter_writef8(emitter, PNE_SMALLUINT);
    pni_emitter_writef8(emitter, uint);
  } else {
    pni_emitter_writef8(emitter, PNE_UINT);
    pni_emitter_writef32(emitter, uint);
  }
  compound->count++;
}

static inline void emit_ulong(pni_emitter_t* emitter, pni_compound_context* compound, uint64_t ulong) {
  emit_accumulated_nulls(emitter, compound);
  if (ulong == 0) {
    pni_emitter_writef8(emitter, PNE_ULONG0);
  } else if (ulong < 256) {
    pni_emitter_writef8(emitter, PNE_SMALLULONG);
    pni_emitter_writef8(emitter, ulong);
  } else {
    pni_emitter_writef8(emitter, PNE_ULONG);
    pni_emitter_writef64(emitter, ulong);
  }
  compound->count++;
}

static inline void emit_timestamp(pni_emitter_t* emitter, pni_compound_context* compound, pn_timestamp_t timestamp) {
  emit_accumulated_nulls(emitter, compound);
  pni_emitter_writef8(emitter, PNE_MS64);
  pni_emitter_writef64(emitter, timestamp);
  compound->count++;
}

static inline void emit_uuid(pni_emitter_t* emitter, pni_compound_context* compound, pn_uuid_t* uuid) {
  emit_accumulated_nulls(emitter, compound);
  pni_emitter_writef8(emitter, PNE_UUID);
  pni_emitter_writef128(emitter, uuid);
  compound->count++;
}

static inline void emit_descriptor(pni_emitter_t* emitter, pni_compound_context* compound, uint64_t ulong) {
  emit_accumulated_nulls(emitter, compound);
  pni_emitter_writef8(emitter, PNE_DESCRIPTOR);
  if (ulong < 256) {
    pni_emitter_writef8(emitter, PNE_SMALLULONG);
    pni_emitter_writef8(emitter, ulong);
  } else {
    pni_emitter_writef8(emitter, PNE_ULONG);
    pni_emitter_writef64(emitter, ulong);
  }
}

static inline pni_compound_context emit_list(pni_emitter_t* emitter, pni_compound_context* compound, bool small_encoding, bool is_described_list) {
  emit_accumulated_nulls(emitter, compound);
  if (small_encoding) {
    pni_emitter_writef8(emitter, PNE_LIST8);
    // Need to fill in size and count later
    size_t size_position = emitter->position;
    pni_emitter_writef8(emitter, 0);
    size_t start_position = emitter->position;
    pni_emitter_writef8(emitter, 0);
    return (pni_compound_context){
      .previous_compound = compound,
      .size_position = size_position,
      .start_position = start_position,
      .count = 0,
      .null_count = 0,
      .is_described_list = is_described_list,
    };
  } else {
    pni_emitter_writef8(emitter, PNE_LIST32);
    // Need to fill in size and count later
    size_t size_position = emitter->position;
    pni_emitter_writef32(emitter, 0);
    size_t start_position = emitter->position;
    pni_emitter_writef32(emitter, 0);
    return (pni_compound_context){
      .previous_compound = compound,
      .size_position = size_position,
      .start_position = start_position,
      .count = 0,
      .null_count = 0,
      .is_described_list = is_described_list,
    };
  }
}

static inline void emit_end_list(pni_emitter_t* emitter, pni_compound_context* compound, bool small_encoding) {
  // Check if list was 0 length
  if (compound->count==0) {
    emitter->position = compound->size_position - 1;
    pni_emitter_writef8(emitter, PNE_LIST0);
    compound->previous_compound->count++;
    compound->encoding_succeeded = true;
    return;
  }
  // Fill in size and count
  size_t current = emitter->position;
  emitter->position = compound->size_position;
  size_t size = current-compound->start_position;
  if (small_encoding) {
    if (size>255 || compound->count>255) {
      compound->encoding_succeeded = false;
      emitter->position -= 1;
      return;
    }
    pni_emitter_writef8(emitter, size);
    pni_emitter_writef8(emitter, compound->count);
  } else {
    pni_emitter_writef32(emitter, size);
    pni_emitter_writef32(emitter, compound->count);
  }
  emitter->position = current;
  compound->previous_compound->count++;
  compound->encoding_succeeded = true;
}

static inline pni_compound_context emit_array(pni_emitter_t* emitter, pni_compound_context* compound, bool small_encoding, pn_type_t type) {
  emit_accumulated_nulls(emitter, compound);
  if (small_encoding) {
    pni_emitter_writef8(emitter, PNE_ARRAY8);
    // Need to fill in size, count and type later
    size_t size_position = emitter->position;
    pni_emitter_writef8(emitter, 0);
    size_t start_position = emitter->position;
    pni_emitter_writef8(emitter, 0);
    pni_emitter_writef8(emitter, 0);
    return (pni_compound_context){
      .previous_compound = compound,
      .size_position = size_position,
      .start_position = start_position,
      .count = 0,
    };
  } else {
    pni_emitter_writef8(emitter, PNE_ARRAY32);
    // Need to fill in size, count and type later
    size_t size_position = emitter->position;
    pni_emitter_writef32(emitter, 0);
    size_t start_position = emitter->position;
    pni_emitter_writef32(emitter, 0);
    pni_emitter_writef8(emitter, 0);
    return (pni_compound_context){
      .previous_compound = compound,
      .size_position = size_position,
      .start_position = start_position,
      .count = 0,
    };
  }
}

static inline void emit_end_array(pni_emitter_t* emitter, pni_compound_context* compound, bool small_encoding) {
  // Fill in size, count and type
  size_t current = emitter->position;
  emitter->position = compound->size_position;
  size_t size = current-compound->start_position;
  if (small_encoding) {
    if (size>255 || compound->count>255) {
      compound->encoding_succeeded = false;
      emitter->position -= 1;
      return;
    }
    pni_emitter_writef8(emitter, current-compound->start_position);
    pni_emitter_writef8(emitter, compound->count);
    pni_emitter_writef8(emitter, compound->type);
  } else {
    pni_emitter_writef32(emitter, current-compound->start_position);
    pni_emitter_writef32(emitter, compound->count);
    pni_emitter_writef8(emitter, compound->type);
  }
  emitter->position = current;
  compound->previous_compound->count++;
  compound->encoding_succeeded = true;
}

static inline void emit_binary_bytes(pni_emitter_t* emitter, pni_compound_context* compound, pn_bytes_t bytes) {
  emit_accumulated_nulls(emitter, compound);
  if (bytes.size < 256) {
    pni_emitter_writef8(emitter, PNE_VBIN8);
    pni_emitter_writev8(emitter, bytes);
  } else {
    pni_emitter_writef8(emitter, PNE_VBIN32);
    pni_emitter_writev32(emitter, bytes);
  }
  compound->count++;
}

static inline void emit_string_bytes(pni_emitter_t* emitter, pni_compound_context* compound, pn_bytes_t bytes) {
  emit_accumulated_nulls(emitter, compound);
  if (bytes.size < 256) {
    pni_emitter_writef8(emitter, PNE_STR8_UTF8);
    pni_emitter_writev8(emitter, bytes);
  } else {
    pni_emitter_writef8(emitter, PNE_STR32_UTF8);
    pni_emitter_writev32(emitter, bytes);
  }
  compound->count++;
}

static inline void emit_symbol_bytes(pni_emitter_t* emitter, pni_compound_context* compound, pn_bytes_t bytes) {
  emit_accumulated_nulls(emitter, compound);
  if (bytes.size < 256) {
    pni_emitter_writef8(emitter, PNE_SYM8);
    pni_emitter_writev8(emitter, bytes);
  } else {
    pni_emitter_writef8(emitter, PNE_SYM32);
    pni_emitter_writev32(emitter, bytes);
  }
  compound->count++;
}

static inline void emit_symbol(pni_emitter_t* emitter, pni_compound_context* compound, pn_bytes_t bytes) {
  if (bytes.start == NULL) {
    emit_null(emitter, compound);
  } else {
    emit_symbol_bytes(emitter, compound, bytes);
  }
}

static inline void emit_string(pni_emitter_t* emitter, pni_compound_context* compound,  pn_bytes_t bytes) {
  if (bytes.start == NULL) {
    emit_null(emitter, compound);
  } else {
    emit_string_bytes(emitter, compound, bytes);
  }
}

static inline void emit_binarynonull(pni_emitter_t* emitter, pni_compound_context* compound, size_t size, const char* bytes) {
  emit_binary_bytes(emitter, compound, (pn_bytes_t){.size = size, .start = bytes});
}

static inline void emit_binaryornull(pni_emitter_t* emitter, pni_compound_context* compound, size_t size, const char* bytes) {
  if (bytes == NULL) {
    emit_null(emitter, compound);
  } else {
    emit_binary_bytes(emitter, compound, (pn_bytes_t){.size = size, .start = bytes});
  }
}

static inline void emit_atom(pni_emitter_t* emitter, pni_compound_context* compound, pn_atom_t* atom) {
  switch (atom->type) {
    default:
    case PN_NULL:
      emit_null(emitter, compound);
      return;
    case PN_BOOL:
      emit_bool(emitter, compound, atom->u.as_bool);
      return;
    case PN_UBYTE:
      emit_ubyte(emitter, compound, atom->u.as_ubyte);
      return;
    case PN_USHORT:
      emit_ushort(emitter, compound, atom->u.as_ushort);
      return;
    case PN_UINT:
      emit_uint(emitter, compound, atom->u.as_uint);
      return;
    case PN_ULONG:
      emit_ulong(emitter, compound, atom->u.as_ulong);
      return;
    case PN_TIMESTAMP:
      emit_timestamp(emitter, compound, atom->u.as_timestamp);
      return;
    case PN_UUID:
      emit_uuid(emitter, compound, &atom->u.as_uuid);
      return;
    case PN_BINARY:
      emit_binary_bytes(emitter, compound, atom->u.as_bytes);
      return;
    case PN_STRING:
      emit_string_bytes(emitter, compound, atom->u.as_bytes);
      return;
    case PN_SYMBOL:
      emit_symbol_bytes(emitter, compound, atom->u.as_bytes);
      return;
  }
}

// NB: This function is only correct because it currently can only be called to fill out an array
static inline void emit_counted_symbols(pni_emitter_t* emitter, pni_compound_context* compound, size_t count, char** symbols) {
  // 64 is a heuristic - 64 3 character symbols will already be 256 bytes
  if (count>64){
    compound->type = PNE_SYM32;
    for (size_t i=0; i<count; ++i) {
      size_t size = strlen(symbols[i]);
      pni_emitter_writev32(emitter, (pn_bytes_t){.size = size, .start = symbols[i]});
    }
    compound->count+=count;
    return;
  }

  pn_bytes_t bsymbols[64];
  size_t total_bytes = 0;
  for (size_t i=0; i<count; ++i) {
    // FIXME: Yuck we need to use strlen to find the end of the strings - fix this by changing the signature to take pn_bytes_t[]
    size_t size = strlen(symbols[i]);
    bsymbols[i] = (pn_bytes_t){.size = size, .start = symbols[i]};
    total_bytes += size+1; // Assuming PNE_SYM8 encoding
  }
  if (total_bytes<256) {
    compound->type = PNE_SYM8;
    for (size_t i=0; i<count; ++i) {
      pni_emitter_writev8(emitter, bsymbols[i]);
    }
  } else {
    compound->type = PNE_SYM32;
    for (size_t i=0; i<count; ++i) {
      pni_emitter_writev32(emitter, bsymbols[i]);
    }
  }
  compound->count+=count;
}

static inline void pni_emitter_data(pni_emitter_t* emitter, pn_data_t* data) {
  ssize_t data_size = 0;
  if (emitter->position >= emitter->size ||
      PN_OVERFLOW == (data_size = pn_data_encode(data, emitter->output_start+emitter->position, emitter->size-emitter->position))) {
    emitter->position += pn_data_encoded_size(data);
  } else {
    emitter->position += data_size;
  }
}

static inline void emit_copy(pni_emitter_t* emitter, pni_compound_context* compound, pn_data_t* data) {
  if (!data || pn_data_size(data) == 0) {
    emit_null(emitter, compound);
    return;
  }

  emit_accumulated_nulls(emitter, compound);
  pn_handle_t point = pn_data_point(data);
  pn_data_rewind(data);
  pni_emitter_data(emitter, data);
  pn_data_restore(data, point);
  compound->count++;
}

static inline void emit_raw(pni_emitter_t* emitter, pni_compound_context* compound, const pn_bytes_t bytes) {
  if (bytes.size==0 || bytes.start == 0) {
    emit_null(emitter, compound);
    return;
  }

  emit_accumulated_nulls(emitter, compound);
  pni_emitter_raw(emitter, bytes);
  compound->count++;
}

// Keep this here as a placeholder until we do something more intelligent
static inline void emit_multiple(pni_emitter_t* emitter, pni_compound_context* compound, const pn_bytes_t bytes) {
  emit_raw(emitter, compound, bytes);
}

static inline void emit_described_type_raw(pni_emitter_t* emitter, pni_compound_context* compound, uint64_t descriptor, const pn_bytes_t bytes) {
  emit_descriptor(emitter, compound, descriptor);
  pni_compound_context c = make_compound();
  emit_raw(emitter, &c, bytes);
  // Can only be a single item (probably a list though)
  compound->count++;
}

static inline void emit_copy_or_raw(pni_emitter_t* emitter, pni_compound_context* compound, pn_data_t* data, pn_bytes_t raw)
{
  if (data) {
    emit_copy(emitter, compound, data);
  } else {
    emit_raw(emitter, compound, raw);
  }
}

static inline void emit_list0(pni_emitter_t* emitter, pni_compound_context* compound)
{
  pni_emitter_writef8(emitter, PNE_LIST0);
  compound->count++;
}

static inline void emit_condition(pni_emitter_t* emitter, pni_compound_context* compound0, pn_condition_t* condition) {
  if (!condition || !condition->name || !pn_string_get(condition->name)) {
    emit_null(emitter, compound0);
    return;
  }

  emit_descriptor(emitter, compound0, ERROR);
  for (bool small_encoding = true; ; small_encoding = false) {
    pni_compound_context c = emit_list(emitter, compound0, small_encoding, true);
    pni_compound_context compound = c;
    pn_bytes_t name_bytes = pn_string_bytes(condition->name);
    if (name_bytes.size==0) {
      emit_null(emitter, &compound);
    } else {
      emit_symbol_bytes(emitter, &compound, name_bytes);
    }
    pn_bytes_t description_bytes = pn_string_bytes(condition->description);
    if (description_bytes.size==0) {
      emit_null(emitter, &compound);
    } else {
      emit_string_bytes(emitter, &compound, description_bytes);
    }
    emit_copy_or_raw(emitter, &compound, condition->info, condition->info_raw);
    emit_end_list(emitter, &compound, small_encoding);
    if (encode_succeeded(emitter, &compound)) break;
  }
}

static inline void emit_disposition(pni_emitter_t* emitter, pni_compound_context* compound0, pn_disposition_t *disposition)
{
  if (!disposition || !disposition->type) {
    emit_null(emitter, compound0);
    return;
  }

  emit_descriptor(emitter, compound0, disposition->type);
  switch (disposition->type) {
    case PN_RECEIVED:
      for (bool small_encoding = true; ; small_encoding = false) {
        pni_compound_context c = emit_list(emitter, compound0, small_encoding, true);
        pni_compound_context compound = c;
        emit_uint(emitter, &compound, disposition->section_number);
        emit_ulong(emitter, &compound, disposition->section_offset);
        emit_end_list(emitter, &compound, small_encoding);
        if (encode_succeeded(emitter, &compound)) break;
      }
      return;
    case PN_ACCEPTED:
    case PN_RELEASED:
      emit_list0(emitter, compound0);
      return;
    case PN_REJECTED:
      for (bool small_encoding = true; ; small_encoding = false) {
        pni_compound_context c = emit_list(emitter, compound0, small_encoding, true);
        pni_compound_context compound = c;
        emit_condition(emitter, &compound, &disposition->condition);
        emit_end_list(emitter, &compound, small_encoding);
        if (encode_succeeded(emitter, &compound)) break;
      }
      return;
    case PN_MODIFIED:
      for (bool small_encoding = true; ; small_encoding = false) {
        pni_compound_context c = emit_list(emitter, compound0, small_encoding, true);
        pni_compound_context compound = c;
        emit_bool(emitter, &compound, disposition->failed);
        emit_bool(emitter, &compound, disposition->undeliverable);
        emit_copy_or_raw(emitter, &compound, disposition->annotations, disposition->annotations_raw);
        emit_end_list(emitter, &compound, small_encoding);
        if (encode_succeeded(emitter, &compound)) break;
      }
      return;
    default:
      if ((disposition->data && pn_data_size(disposition->data) == 0) ||
          (!disposition->data && disposition->data_raw.size == 0)) {
        emit_list0(emitter, compound0);
        return;
      }
      pni_compound_context c = make_compound();
      emit_copy_or_raw(emitter, &c, disposition->data, disposition->data_raw);
      compound0->count++;
      return;
  }
}

#endif // PROTON_EMITTERS_H
