#ifndef _PROTON_DATA_H
#define _PROTON_DATA_H 1

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
#include "buffer.h"
#include "decoder.h"
#include "encoder.h"

typedef uint16_t pni_nid_t;
#define PNI_NID_MAX ((pni_nid_t)-1)
#define PNI_INTERN_MINSIZE 64

/*
 * Value payload for a pni_node_t.
 *
 * BINARY/STRING/SYMBOL/DECIMAL128/UUID nodes store their data in the intern
 * buffer (data->buf); as_bytes.offset and as_bytes.size locate the bytes.
 * DECIMAL128 and UUID always have as_bytes.size == 16.
 *
 * PN_ARRAY nodes use as_array: element type, whether the array has a
 * descriptor child (described), and encoder scratch (start, small).
 *
 * PN_LIST nodes use as_list: whether the list is the body of a described
 * value (controls trailing-null elision during encoding), and encoder
 * scratch (start, small).
 *
 * PN_MAP nodes use as_map: encoder scratch (start, small).
 *
 * All other types (NULL, DESCRIBED) carry no payload; only the type tag
 * on pni_node_t is meaningful.
 */
typedef union {
  bool            as_bool;
  uint8_t         as_ubyte;
  int8_t          as_byte;
  uint16_t        as_ushort;
  int16_t         as_short;
  uint32_t        as_uint;
  int32_t         as_int;
  uint32_t        as_char;        /* pn_char_t is typedef'd uint32_t */
  uint64_t        as_ulong;
  int64_t         as_long;
  int64_t         as_timestamp;   /* pn_timestamp_t is typedef'd int64_t */
  float           as_float;
  double          as_double;
  uint32_t        as_decimal32;
  uint64_t        as_decimal64;
  struct {
    uint32_t      offset;         /* byte offset into data->buf */
    uint32_t      size;           /* byte count (always 16 for decimal128/uuid) */
  }               as_bytes;
  struct {
    uint32_t      start;          /* encoder scratch: output offset of size field */
    bool          described;      /* true if first child is a descriptor */
    uint8_t       type;           /* element type (pn_type_t fits in uint8_t: values 1-25) */
    /* 2 implicit padding bytes */
  }               as_array;
  struct {
    uint32_t      start;          /* encoder scratch: output offset of size field */
    bool          described;      /* true if body of a described composite */
    /* 3 implicit padding bytes */
  }               as_list;
  struct {
    uint32_t      start;          /* encoder scratch: output offset of size field */
    /* 4 implicit padding bytes */
  }               as_map;
} pni_node_payload_t;

/*
 * Layout (64-bit): 24 bytes.
 *
 *  offset  0  type        (4)  value type tag
 *  offset  4  next        (2)  sibling link
 *  offset  6  prev        (2)  sibling link
 *  offset  8  down        (2)
 *  offset 10  parent      (2)
 *  offset 12  children    (2)
 *  offset 14  <2 implicit alignment bytes before u>
 *  offset 16  u           (8)  value payload (8-byte aligned)
 *
 * Note that there is still a bit of possibility to reduce this further:
 * The type takes 4 bytes, but need only take 1; there is 2 bytes of padding still;
 * I'm pretty sure that we could do away with one of the navigation links (having down
 * and children seems redundant). However unless we can get it to 16 bytes there is
 * little point as it must be 8 byte aligned anyway (because of the int64_t in the union).
 */
typedef struct {
  pn_type_t           type;
  pni_nid_t           next;
  pni_nid_t           prev;
  pni_nid_t           down;
  pni_nid_t           parent;
  pni_nid_t           children;
  pni_node_payload_t  u;
} pni_node_t;

struct pn_data_t {
  pni_node_t *nodes;
  pn_buffer_t *buf;
  pn_error_t *error;
  size_t max_buf_size; /* intern buffer limit during decode; 0 = unlimited */
  pni_nid_t max_nid;   /* node count limit during decode; 0 = unlimited */
  pni_nid_t capacity;
  pni_nid_t size;
  pni_nid_t parent;
  pni_nid_t current;
  pni_nid_t base_parent;
  pni_nid_t base_current;
};

/* Node-count limits for pni_switch_to_data().
 * 0-width elements (e.g. PNE_NULL) consume a node but no bytes, so bytes->size
 * alone does not bound node count — hence a separate constant is needed.
 *
 * DEFAULT (1024): performative fields (properties, capabilities, annotations,
 *   condition info, disposition data).
 * BODY (0 = unlimited): message body — application data whose node count is
 *   only bounded by the uint16 hard ceiling of PNI_NID_MAX.
 */
#define PNI_DATA_DEFAULT_MAX_NODES 1024
#define PNI_DATA_BODY_MAX_NODES    0

static inline pni_node_t * pn_data_node(pn_data_t *data, pni_nid_t nd)
{
  return nd ? (data->nodes + nd - 1) : NULL;
}

int pni_data_traverse(pn_data_t *data,
                      int (*enter)(void *ctx, pn_data_t *data, pni_node_t *node),
                      int (*exit)(void *ctx, pn_data_t *data, pni_node_t *node),
                      void *ctx);

struct pn_fixed_string_t;
void pni_inspect_atom(pn_atom_t *atom, struct pn_fixed_string_t *str);

#endif /* data.h */
