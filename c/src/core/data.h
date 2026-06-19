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

typedef struct {
  size_t start;
  size_t data_offset;
  size_t data_size;
  pn_atom_t atom;
  pn_type_t type;
  pni_nid_t next;
  pni_nid_t prev;
  pni_nid_t down;
  pni_nid_t parent;
  pni_nid_t children;
  // for arrays
  bool described;
  bool data;
  bool small;
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
