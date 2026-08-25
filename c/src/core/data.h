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
 * Bookkeeping the decoder keeps in a compound node while it is being decoded.
 * It is dead once the node's last child has been decoded, so it can live in
 * the node's scratch space. remaining is a pni_nid_t because a node can never
 * have more children than the node array can hold.
 */
typedef struct {
  pni_nid_t remaining;  /* children still to be decoded */
  uint8_t   typecode;   /* constructor shared by all elements (arrays only) */
  uint8_t   unused;
} pni_decoder_state_t;

/*
 * Value payload for a pni_node_t.
 *
 * BINARY/STRING/SYMBOL/DECIMAL128/UUID nodes store their data in the intern
 * buffer (data->buf); as_bytes.offset and as_bytes.size locate the bytes.
 * DECIMAL128 and UUID always have as_bytes.size == 16.
 *
 * Compound nodes share a single layout for down/children and scratch state:
 * PN_ARRAY, PN_ARRAY_DESCRIBED, PN_LIST, PN_MAP and PN_DESCRIBED all use
 * as_compound.
 *
 * All other types carry no payload; only the type tag on pni_node_t is
 * meaningful.
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

  // Compound types share the same navigation and scratch layout.
  struct {
    pni_nid_t down;            // offset 0: 2 bytes
    pni_nid_t children_count;  // offset 2: 2 bytes
    union {
      uint32_t            as_u32;           /* encoder: where the node's header was written */
      pni_decoder_state_t as_decoder_state; /* decoder: what is left to decode */
    } scratch;                // offset 4: 4 bytes
  }               as_compound;     // 8 bytes
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

/* The type of the node we are currently inside, PN_INVALID at the top level.
 * This can be an internal type, e.g. PN_ARRAY_DESCRIBED. */
static inline pn_type_t pni_data_parent_type(pn_data_t *data)
{
  pni_node_t *node = pn_data_node(data, data->parent);
  return node ? (pn_type_t) node->type : PN_INVALID;
}

static inline pni_nid_t pni_node_get_down(pni_node_t *node)
{
  if (!node) return 0;
  switch (node->type) {
    case PN_ARRAY:
    case PN_ARRAY_DESCRIBED:
    case PN_LIST:
    case PN_MAP:
    case PN_DESCRIBED:
      return node->u.as_compound.down;
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
    case PN_LIST:
    case PN_MAP:
    case PN_DESCRIBED:
      node->u.as_compound.down = down;
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
    case PN_LIST:
    case PN_MAP:
    case PN_DESCRIBED:
      return node->u.as_compound.children_count;
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
    case PN_LIST:
    case PN_MAP:
    case PN_DESCRIBED:
      node->u.as_compound.children_count = count;
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
    case PN_LIST:
    case PN_MAP:
    case PN_DESCRIBED:
      node->u.as_compound.children_count++;
      break;
    case PN_DEFER:
      node->u.as_deferred.children_count++;
      break;
    default:
      break;
  }
}

/* Set the element type of the array we are currently inside (data->parent).
 * The decoder must create an array node before it has read the constructor
 * that gives the element type, so it fills the type in afterwards. */
void pni_data_set_parent_array_type(pn_data_t *data, pn_type_t type);
int pni_data_traverse(pn_data_t *data,
                      int (*enter)(void *ctx, pn_data_t *data, pni_node_t *node),
                      int (*exit)(void *ctx, pn_data_t *data, pni_node_t *node),
                      void *ctx);

struct pn_fixed_string_t;
void pni_inspect_atom(pn_atom_t *atom, struct pn_fixed_string_t *str);

#endif /* data.h */
