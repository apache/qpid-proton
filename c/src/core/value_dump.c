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

#include "value_dump.h"

#include "encodings.h"
#include "consumers.h"
#include "fixed_string.h"
#include "framing.h"
#include "protocol.h"
#include "util.h"

#include "proton/types.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdint.h>

static inline pn_bytes_t pn_bytes_advance(pn_bytes_t bytes, size_t size) {
  return (pn_bytes_t) {.size=bytes.size-size,.start=bytes.start+size};
}

// This is only used in places where a described value is not wanted/allowed
// So we interpret type to mean the 'base' type of the described value in this case
static inline void pni_frame_get_type_value2(pni_consumer_t* consumer, uint8_t* type, pn_bytes_t* value) {
  if (!pni_consumer_readf8(consumer, type)) goto error;
  if (*type==PNE_DESCRIPTOR) {
    // Skip over descriptor potentially recursively
    uint8_t dtype;
    pn_bytes_t dvalue;
    pni_frame_get_type_value2(consumer, &dtype, &dvalue);
    if (!pni_consumer_readf8(consumer, type)) goto error;
  }
  if (!pni_consumer_read_value_not_described(consumer, *type, value)) goto error;
  return;

error:
  *value = (pn_bytes_t){0,0};
}

static inline size_t pni_frame_get_type_value(pn_bytes_t bytes, uint8_t* type, pn_bytes_t* value) {
  pni_consumer_t consumer = make_consumer_from_bytes(bytes);
  pni_frame_get_type_value2(&consumer, type, value);
  return consumer.position;
}

static inline size_t pni_frame_read_value_not_described(pn_bytes_t bytes, uint8_t type, pn_bytes_t* value) {
  pni_consumer_t consumer = make_consumer_from_bytes(bytes);
  if (!pni_consumer_read_value_not_described(&consumer, type, value)) {
    *value = (pn_bytes_t){0,0};
  };
  return consumer.position;
}

static inline bool type_isfixedsize(uint8_t type) {
  return type < 0xa0;
}

static inline bool type_is8bitsize(uint8_t type) {
  uint8_t subcategory = type >> 4;
  return subcategory==0xA || subcategory==0xC || subcategory==0xE;
}

static inline bool type_isspecial(uint8_t type) {
  return (type >> 4) == 0x4;
}

static inline bool type_issimpleint(uint8_t type) {
  uint8_t subcategory = type >> 4;
  return (subcategory==0x5 && type<=0x55) || ((subcategory>0x5 && subcategory<=0x8) && (type & 0xe) == 0);
}

static inline bool type_isunsigned_ifsimpleint(uint8_t type) {
  uint8_t subtype = type & 0xf;
  return (subtype==0 || subtype==2 || subtype==3);
}

static inline bool type_isulong(uint8_t type) {
  return type==PNE_ULONG0 || type==PNE_SMALLULONG || type==PNE_ULONG0;
}

static inline bool type_iscompund(uint8_t type) {
  return type >= 0xc0;
}

static inline bool type_islist_notspecial(uint8_t type) {
  return type==PNE_LIST8 || type==PNE_LIST32;
}

void pn_value_dump_special(uint8_t type, pn_fixed_string_t *output) {
  switch (type) {
    case PNE_NULL:
      pn_fixed_string_addf(output, "null");
      break;
    case PNE_TRUE:
      pn_fixed_string_addf(output, "true");
      break;
    case PNE_FALSE:
      pn_fixed_string_addf(output, "false");
      break;
    case PNE_UINT0:
      pn_fixed_string_addf(output, "0x0");
      break;
    case PNE_ULONG0:
      pn_fixed_string_addf(output, "0x0");
      break;
    case PNE_LIST0:
      pn_fixed_string_addf(output, "[]");
      break;
    default:
      pn_fixed_string_addf(output, "!!<unknown>");
      break;
  }
}

void pn_value_dump_descriptor_ulong(uint8_t type, pn_bytes_t value, pn_fixed_string_t* output, uint64_t* dcode) {
  uint64_t ulong;
  switch (type) {
    case PNE_ULONG0:
      ulong = 0;
      break;
    case PNE_SMALLULONG:
      ulong = *value.start;
      break;
    case PNE_ULONG:
      ulong = pni_read64(value.start);
      break;
    default:
      // If we get a different descriptor type - huh
      pn_fixed_string_addf(output, "!!<not-a-ulong>");
      return;
  }
  *dcode = ulong;

  // Check if we have a name for this descriptor
  if (ulong>=FIELD_MIN && ulong<=FIELD_MAX) {
    uint8_t name_index = FIELDS[ulong-FIELD_MIN].name_index;
    if (name_index!=0) {
      pn_fixed_string_addf(output, "%s(%" PRIu64 ") ",
                     (const char *)FIELD_STRINGPOOL.STRING0+FIELD_NAME[name_index],
                     ulong);
      return;
    }
  }

  pn_fixed_string_addf(output, "%" PRIu64 " ", ulong);
  return;
}

void pn_value_dump_scalar(uint8_t type, pn_bytes_t value, pn_fixed_string_t *output){
  if (type_isfixedsize(type)) {
    if (type_isspecial(type)) {
      pn_value_dump_special(type, output);
    } else if (type_issimpleint(type)) {
      // Read bits into unsigned sign extended
      uint64_t uint;
      uint64_t mask;
      switch (value.size) {
        case 1:
          uint = (int8_t)*value.start;
          mask = 0xff;
          break;
        case 2:
          uint = (int16_t)pni_read16(value.start);
          mask = 0xffff;
          break;
        case 4:
          uint = (int32_t)pni_read32(value.start);
          mask = 0xffffffff;
          break;
        case 8:
          uint = pni_read64(value.start);
          mask = 0xffffffffffffffff;
          break;
        case 0:
          // We'll get here if there aren't enough bytes left for the value
          pn_fixed_string_addf(output, "!!");
          return;
        default:
          // It has to be length 1,2,4 or 8!
          pn_fixed_string_addf(output, "!!<WeirdLengthHappened(%zu)>", value.size);
          return;
      }
      if (type_isunsigned_ifsimpleint(type)) {
        // mask high sign extended bits if unsigned
        uint = uint & mask;
        pn_fixed_string_addf(output, "0x%" PRIx64, uint);
      } else {
        int64_t i = (int64_t)uint;
        pn_fixed_string_addf(output, "%" PRIi64, i);
      }
    } else {
      // Check if we didn't have enough bytes for the value
      if (value.size==0) {
        pn_fixed_string_addf(output, "!!");
        return;
      }
      switch (type) {
        case PNE_BOOLEAN:
          pn_fixed_string_addf(output, *value.start ? "true" : "false");
          break;
        case PNE_FLOAT: {
          union {uint32_t i; float f;} conv;
          conv.i = pni_read32(value.start);
          pn_fixed_string_addf(output, "%g", conv.f);
          break;
        }
        case PNE_DOUBLE: {
          union {uint64_t i; double f;} conv;
          conv.i = pni_read64(value.start);
          pn_fixed_string_addf(output, "%g", conv.f);
          break;
        }
        case PNE_UTF32:
          break;
        case PNE_MS64: {
          int64_t timestamp = pni_read64(value.start);
          pn_fixed_string_addf(output, "%" PRIi64, timestamp);
          break;
        }
        case PNE_DECIMAL32:
          pn_fixed_string_addf(output, "D32(%04" PRIx32 ")", pni_read32(value.start));
          break;
        case PNE_DECIMAL64:
          pn_fixed_string_addf(output, "D64(%08" PRIx64 ")", pni_read64(value.start));
          break;
        case PNE_DECIMAL128:
          pn_fixed_string_addf(
            output,
            "D128(%08" PRIx64 "%08" PRIx64 ")",
                  pni_read64(value.start), pni_read64(&value.start[8]));
          break;
        case PNE_UUID:
          pn_fixed_string_addf(
            output,
            "UUID(%02hhx%02hhx%02hhx%02hhx-"
            "%02hhx%02hhx-%02hhx%02hhx-%02hhx%02hhx-"
            "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx)",
            value.start[0], value.start[1], value.start[2],value.start[3],
            value.start[4], value.start[5],
            value.start[6], value.start[7],
            value.start[8], value.start[9],
            value.start[10], value.start[11], value.start[12], value.start[13], value.start[14], value.start[15]);
          break;
        default:
          pn_fixed_string_addf(output, "!!<UnknownType<0x%02hhx>(", type);
          for (size_t i=0; i<value.size; i++) {
            pn_fixed_string_addf(output, "%.2x", value.start[i]);
          }
          pn_fixed_string_addf(output, ")>");
      }
    }
  } else {
    // given we're variable size scalar and we've already calculated the length we can simply switch on subtype
    // which can only be 0 (binary), 1 (utf8 string) or 3 (symbol) [I wonder what happened to subtype 2!]
    const char* prefix = "";
    const char* suffix = "";
    switch (type & 0xf) {
      case 0:
        prefix = "b\"";
        suffix = "\"";
        break;
      case 1:
        prefix = "\"";
        suffix = "\"";
        break;
      case 3: {
        bool quote = false;
        if (!isalpha(value.start[0])) {
          quote = true;
        } else {
          for (size_t i = 1; i < value.size; i++) {
            if ( !isalnum(value.start[i]) && value.start[i]!='-' ) {
              quote = true;
              break;
            }
          }
        }
        if (quote) {
          prefix = ":\"";
          suffix = "\"";
        } else {
          prefix = ":";
          suffix = "";
        }
        break;
      }
      default:
        prefix = "<?<";
        suffix = ">?>";
    }
    pn_fixed_string_addf(output, "%s", prefix);
    pn_fixed_string_quote(output, value.start, value.size);
    pn_fixed_string_addf(output, "%s", suffix);
  }
}

static inline uint32_t consume_count(uint8_t type, pn_bytes_t *value) {
  uint32_t count;
  if (type_is8bitsize(type)) {
    count = value->start[0];
    *value = pn_bytes_advance(*value, 1);
  } else {
    count = pni_read32(value->start);
    *value = pn_bytes_advance(*value, 4);
  }
  return count;
}

void pn_value_dump_nondescribed_value(uint8_t type, pn_bytes_t value, pn_fixed_string_t *output);

void pn_value_dump_list(uint32_t count, pn_bytes_t value, pn_fixed_string_t *output) {
  uint32_t elements = 0;
  pn_fixed_string_addf(output, "[");
  while (value.size) {
    elements++;
    size_t size = pni_value_dump(value, output);
    value = pn_bytes_advance(value, size);
    if (value.size) {
      pn_fixed_string_addf(output, ", ");
    }
  }
  pn_fixed_string_addf(output, "]");
  if (elements!=count) {
    pn_fixed_string_addf(output, "<%" PRIu32 "!=%" PRIu32 ">", elements, count);
  }
}

void pn_value_dump_described_list(uint32_t count, pn_bytes_t value, uint64_t dcode, pn_fixed_string_t *output) {
  uint32_t elements = 0;
  bool output_element = false;
  pn_fixed_string_addf(output, "[");
  while (value.size) {
    uint8_t type = value.start[0];
    if (type==PNE_NULL) {
      value = pn_bytes_advance(value, 1);
    } else {
      if (output_element) {
        pn_fixed_string_addf(output, ", ");
      }
      const pn_fields_t *fields = &FIELDS[dcode-FIELD_MIN];
      if (elements < fields->field_count) {
        pn_fixed_string_addf(output, "%s=",
                      (const char*)FIELD_STRINGPOOL.STRING0+FIELD_FIELDS[fields->first_field_index+elements]);
      }
      size_t size = pni_value_dump(value, output);
      value = pn_bytes_advance(value, size);
      output_element = true;
    }
    elements++;
  }
  pn_fixed_string_addf(output, "]");
  if (elements!=count) {
    pn_fixed_string_addf(output, "<%" PRIu32 "!=%" PRIu32 ">", elements, count);
  }
}

void pn_value_dump_map(uint32_t count, pn_bytes_t value, pn_fixed_string_t *output) {
  uint32_t elements = 0;
  pn_fixed_string_addf(output, "{");
  while (value.size) {
    elements++;
    size_t size = pni_value_dump(value, output);
    value = pn_bytes_advance(value, size);
    if (value.size) {
      if (elements & 1) {
        pn_fixed_string_addf(output, "=");
      } else {
        pn_fixed_string_addf(output, ", ");
      }
    }
  }
  pn_fixed_string_addf(output, "}");
  if (elements!=count) {
    pn_fixed_string_addf(output, "<%" PRIu32 "!=%" PRIu32 ">", elements, count);
  }
}

static inline const char* pni_type_name(uint8_t type) {
  switch (type) {
    case PNE_NULL:       return "null";
    case PNE_TRUE:       return "true";
    case PNE_FALSE:      return "false";
    case PNE_BOOLEAN:    return "bool";
    case PNE_BYTE:       return "byte";
    case PNE_UBYTE:      return "ubyte";
    case PNE_SHORT:      return "short";
    case PNE_USHORT:     return "ushort";
    case PNE_INT:
    case PNE_SMALLINT:   return "int";
    case PNE_UINT0:
    case PNE_UINT:
    case PNE_SMALLUINT:  return "uint";
    case PNE_LONG:
    case PNE_SMALLLONG:  return "long";
    case PNE_ULONG0:
    case PNE_ULONG:
    case PNE_SMALLULONG: return "ulong";
    case PNE_FLOAT:      return "float";
    case PNE_DOUBLE:     return "double";
    case PNE_DECIMAL32:  return "decimal32";
    case PNE_DECIMAL64:  return "decimal64";
    case PNE_DECIMAL128: return "decimal128";
    case PNE_UUID:       return "uuid";
    case PNE_MS64:       return "timestamp";
    case PNE_SYM8:
    case PNE_SYM32:      return "symbol";
    case PNE_STR8_UTF8:
    case PNE_STR32_UTF8: return "string";
    case PNE_VBIN8:
    case PNE_VBIN32:     return "binary";
    case PNE_LIST0:
    case PNE_LIST8:
    case PNE_LIST32:     return "list";
    case PNE_MAP8:
    case PNE_MAP32:      return "map";
    default:
      break;
  }
  return NULL;
}

void pn_value_dump_array(uint32_t count, pn_bytes_t value, pn_fixed_string_t *output) {
  // Read base type only (ignoring descriptor) and first value
  uint8_t type = 0;
  pn_bytes_t evalue;
  if (count==0) {
    type = *value.start;
  } else {
    size_t size = pni_frame_get_type_value(value, &type, &evalue);
    value = pn_bytes_advance(value, size);
  }
  if (type==0){
    // Type isn't allowed to be 0 at present because we dump described values as the base type anyway
    pn_fixed_string_addf(output, "@<!!>[]");
    return;
  }
  const char* type_name = pni_type_name(type);
  if (type_name) {
    pn_fixed_string_addf(output, "@<%s>[", type_name);
  } else {
    pn_fixed_string_addf(output, "@<%02hhx>[", type);
  }
  if (count==0) {
    pn_fixed_string_addf(output, "]");
    return;
  }

  pn_value_dump_nondescribed_value(type, evalue, output);
  if (type_isspecial(type)) {
    if (count>1) {
      pn_fixed_string_addf(output, ", ...(%d more)]", count-1);
    } else {
      pn_fixed_string_addf(output, "]");
    }
    return;
  }
  uint32_t elements = 1;
  while (value.size) {
    pn_fixed_string_addf(output, ", ");
    elements++;
    size_t size = pni_frame_read_value_not_described(value, type, &evalue);
    pn_value_dump_nondescribed_value(type, evalue, output);
    if (size <= value.size) {
      value = pn_bytes_advance(value, size);
    } else {
      pn_fixed_string_addf(output, "<error: %zd > %zd>", size, value.size);
      value.size = 0;
    }
  }
  pn_fixed_string_addf(output, "]");
  if (elements!=count) {
    pn_fixed_string_addf(output, "<%" PRIu32 "!=%" PRIu32 ">", elements, count);
  }
}

void pn_value_dump_nondescribed_value(uint8_t type, pn_bytes_t value, pn_fixed_string_t *output){
  if (!type_iscompund(type)) {
    // The one exception to 'scalar' is LIST0 which is encoded as a special type
    pn_value_dump_scalar(type, value, output);
    return;
  }

  if (value.size==0) {
    switch (type) {
      case PNE_LIST8:
      case PNE_LIST32:
        pn_fixed_string_addf(output, "[!!]");
        break;
      case PNE_MAP8:
      case PNE_MAP32:
        pn_fixed_string_addf(output, "{!!}");
        break;
      case PNE_ARRAY8:
      case PNE_ARRAY32:
        pn_fixed_string_addf(output, "@<>[!!]");
        break;
    }
    return;
  }

  // Get count
  uint32_t count = consume_count(type, &value);

  switch (type) {
    case PNE_LIST8:
    case PNE_LIST32:
      pn_value_dump_list(count, value, output);
      break;
    case PNE_MAP8:
    case PNE_MAP32:
      pn_value_dump_map(count, value, output);
      break;
    case PNE_ARRAY8:
    case PNE_ARRAY32:
      pn_value_dump_array(count, value, output);
      break;
  }
}

size_t pn_value_dump_described(pn_bytes_t bytes, uint64_t dcode, pn_fixed_string_t *output) {
  uint8_t type;
  pn_bytes_t value;
  size_t size = pni_frame_get_type_value(bytes, &type, &value);
  if (size==0) {
    pn_fixed_string_addf(output, "!!");
    return 0;
  }

  // The only described type handled differently is list
  if (type_islist_notspecial(type) && dcode) {
    if (value.size==0) {
      pn_fixed_string_addf(output, "[!!]");
      return size;
    }
    uint32_t count = consume_count(type, &value);
    pn_value_dump_described_list(count, value, dcode, output);
  } else {
    pn_value_dump_nondescribed_value(type, value, output);
  }
  return size;
}

size_t pn_value_dump_nondescribed(pn_bytes_t bytes, pn_fixed_string_t *output) {
  uint8_t type;
  pn_bytes_t value;
  size_t size = pni_frame_get_type_value(bytes, &type, &value);
  if (size==0) {
    pn_fixed_string_addf(output, "!!");
    return 0;
  }

  pn_value_dump_nondescribed_value(type, value, output);
  return size;
}

size_t pni_value_dump(pn_bytes_t frame, pn_fixed_string_t *output)
{
  if (frame.size==0) {
    return 0;
  }
  uint8_t type = frame.start[0];
  // Check for described type first
  if (type==PNE_DESCRIPTOR) {
    pn_fixed_string_addf(output, "@");
    frame = pn_bytes_advance(frame, 1);
    size_t fsize = 1;
    uint8_t type;
    pn_bytes_t value;
    size_t size = pni_frame_get_type_value(frame, &type, &value);
    frame = pn_bytes_advance(frame, size);
    fsize += size;
    if (value.size==0) {
      pn_fixed_string_addf(output, "!!");
      return fsize;
    }
    if (type_isulong(type)) {
      uint64_t dcode;
      pn_value_dump_descriptor_ulong(type, value, output, &dcode);
      fsize += pn_value_dump_described(frame, dcode, output);
    } else {
      pn_value_dump_nondescribed_value(type, value, output);
      fsize += pn_value_dump_nondescribed(frame, output);
    }
    return fsize;
  } else {
    return pn_value_dump_nondescribed(frame, output);
  }
}

size_t pn_value_dump(pn_bytes_t frame, char *bytes, uint32_t size)
{
  pn_fixed_string_t output = pn_fixed_string(bytes, size);
  size_t fsize = pni_value_dump(frame, &output);
  pn_fixed_string_terminate(&output);
  return fsize;
}
