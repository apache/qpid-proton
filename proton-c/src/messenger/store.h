#ifndef _PROTON_STORE_H
#define _PROTON_STORE_H 1

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

#include <proton/buffer.h>

typedef struct pni_store_t pni_store_t;
typedef struct pni_entry_t pni_entry_t;

pni_store_t *pni_store(void);
void pni_store_free(pni_store_t *store);
size_t pni_store_size(pni_store_t *store);
pni_entry_t *pni_store_put(pni_store_t *store, const char *address);
pni_entry_t *pni_store_get(pni_store_t *store, const char *address);

pn_buffer_t *pni_entry_bytes(pni_entry_t *entry);
pn_status_t pni_entry_get_status(pni_entry_t *entry);
void pni_entry_set_status(pni_entry_t *entry, pn_status_t status);
pn_delivery_t *pni_entry_get_delivery(pni_entry_t *entry);
void pni_entry_set_delivery(pni_entry_t *entry, pn_delivery_t *delivery);
void pni_entry_set_context(pni_entry_t *entry, void *context);
void *pni_entry_get_context(pni_entry_t *entry);
void pni_entry_updated(pni_entry_t *entry);
void pni_entry_free(pni_entry_t *entry);

pn_sequence_t pni_entry_track(pni_entry_t *entry);
pni_entry_t *pni_store_entry(pni_store_t *store, pn_sequence_t id);
int pni_store_update(pni_store_t *store, pn_sequence_t id, pn_status_t status,
                     int flags, bool settle, bool match);
int pni_store_get_window(pni_store_t *store);
void pni_store_set_window(pni_store_t *store, int window);


#endif /* store.h */
