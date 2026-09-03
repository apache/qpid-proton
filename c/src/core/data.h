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

#define PN_ARRAY_DESCRIBED 26  // Internal type: described array
#define PN_DEFER 27            // Internal type: node used only in pn_data_fill/vfill

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
    pni_nid_t     down;            // offset 0: 2 bytes
    pni_nid_t     children_count;  // offset 2: 2 bytes
    uint8_t       type;           /* deferred type */
  }               as_deferred;

  // Compound types include navigation
  struct {
    pni_nid_t     down;            // offset 0: 2 bytes
    pni_nid_t     children_count;  // offset 2: 2 bytes
    uint32_t      start;           // offset 4: 4 bytes
  }               as_array;        // 8 bytes

  struct {
    pni_nid_t     down;            // offset 0: 2 bytes
    pni_nid_t     children_count;  // offset 2: 2 bytes
    uint32_t      start;           // offset 4: 4 bytes
  }               as_list;         // 8 bytes

  struct {
    pni_nid_t     down;            // offset 0: 2 bytes
    pni_nid_t     children_count;  // offset 2: 2 bytes
    uint32_t      start;           // offset 4: 4 bytes
  }               as_map;          // 8 bytes

  struct {
    pni_nid_t     down;            // offset 0: 2 bytes
    pni_nid_t     children_count;  // offset 2: 2 bytes
  }               as_described;    // 4 bytes (union is 8)
} pni_node_payload_t;

/*
 * Layout (64-bit): 16 bytes.
 *
 *  offset  0  type        (1)  internal value type tag
 *  offset  1  array_type  (1)  array element type (when applicable)
 *  offset  2  next        (2)  sibling link
 *  offset  4  prev        (2)  sibling link
 *  offset  6  parent      (2)  parent link
 *  offset  8  u           (8)  value payload (8-byte aligned)
 *
 * Compound types (PN_ARRAY, PN_LIST, PN_MAP, PN_DESCRIBED) store down/children
 * in their union structure. Scalar types have no children, so down/children are
 * not needed for them.
 */
typedef struct {
  uint8_t             type;        // offset 0: 1 byte
  uint8_t             array_type;  // offset 1: 1 byte
  pni_nid_t           next;        // offset 2: 2 bytes
  pni_nid_t           prev;        // offset 4: 2 bytes
  pni_nid_t           parent;      // offset 6: 2 bytes
  pni_node_payload_t  u;           // offset 8: 8 bytes (8-byte aligned)
} pni_node_t;

#ifdef __cplusplus
static_assert(sizeof(pni_node_t) == 16, "pni_node_t must be 16 bytes");
static_assert(sizeof(pni_node_payload_t) == 8, "union must be 8 bytes");
#else
/* C99 compile-time size assertions */
typedef char pni_node_size_check[sizeof(pni_node_t) == 16 ? 1 : -1];
typedef char pni_payload_size_check[sizeof(pni_node_payload_t) == 8 ? 1 : -1];
#endif

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

static inline pni_nid_t pni_node_get_down(pni_node_t *node)
{
  if (!node) return 0;
  switch (node->type) {
    case PN_ARRAY:
    case PN_ARRAY_DESCRIBED:
      return node->u.as_array.down;
    case PN_LIST:
      return node->u.as_list.down;
    case PN_MAP:
      return node->u.as_map.down;
    case PN_DESCRIBED:
      return node->u.as_described.down;
    case PN_DEFER:
      return node->u.as_deferred.down;
    default:
      return 0;  // Scalar types have no children
  }
}

static inline void pni_node_set_down(pni_node_t *node, pni_nid_t down)
{
  if (!node) return;
  switch (node->type) {
    case PN_ARRAY:
    case PN_ARRAY_DESCRIBED:
      node->u.as_array.down = down;
      break;
    case PN_LIST:
      node->u.as_list.down = down;
      break;
    case PN_MAP:
      node->u.as_map.down = down;
      break;
    case PN_DESCRIBED:
      node->u.as_described.down = down;
      break;
    case PN_DEFER:
      node->u.as_deferred.down = down;
      break;
    default:
      break;  // Scalar types - do nothing
  }
}

static inline pni_nid_t pni_node_get_children(pni_node_t *node)
{
  if (!node) return 0;
  switch (node->type) {
    case PN_ARRAY:
    case PN_ARRAY_DESCRIBED:
      return node->u.as_array.children_count;
    case PN_LIST:
      return node->u.as_list.children_count;
    case PN_MAP:
      return node->u.as_map.children_count;
    case PN_DESCRIBED:
      return node->u.as_described.children_count;
    case PN_DEFER:
      return node->u.as_deferred.children_count;
    default:
      return 0;
  }
}

static inline void pni_node_set_children(pni_node_t *node, pni_nid_t count)
{
  if (!node) return;
  switch (node->type) {
    case PN_ARRAY:
    case PN_ARRAY_DESCRIBED:
      node->u.as_array.children_count = count;
      break;
    case PN_LIST:
      node->u.as_list.children_count = count;
      break;
    case PN_MAP:
      node->u.as_map.children_count = count;
      break;
    case PN_DESCRIBED:
      node->u.as_described.children_count = count;
      break;
    case PN_DEFER:
      node->u.as_deferred.children_count = count;
      break;
    default:
      break;
  }
}

static inline void pni_node_inc_children(pni_node_t *node)
{
  if (!node) return;
  switch (node->type) {
    case PN_ARRAY:
    case PN_ARRAY_DESCRIBED:
      node->u.as_array.children_count++;
      break;
    case PN_LIST:
      node->u.as_list.children_count++;
      break;
    case PN_MAP:
      node->u.as_map.children_count++;
      break;
    case PN_DESCRIBED:
      node->u.as_described.children_count++;
      break;
    case PN_DEFER:
      node->u.as_deferred.children_count++;
      break;
    default:
      break;
  }
}

int pni_data_traverse(pn_data_t *data,
                      int (*enter)(void *ctx, pn_data_t *data, pni_node_t *node),
                      int (*exit)(void *ctx, pn_data_t *data, pni_node_t *node),
                      void *ctx);

struct pn_fixed_string_t;
void pni_inspect_atom(pn_atom_t *atom, struct pn_fixed_string_t *str);

#endif /* data.h */
