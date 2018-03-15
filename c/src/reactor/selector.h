#ifndef PROTON_SELECTOR_H
#define PROTON_SELECTOR_H 1

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

#include <proton/selectable.h>
#include <proton/type_compat.h>

#define PN_READABLE (1)
#define PN_WRITABLE (2)
#define PN_EXPIRED (4)
#define PN_ERROR (8)

/**
 * A ::pn_selector_t provides a selection mechanism that allows
 * efficient monitoring of a large number of Proton connections and
 * listeners.
 *
 * External (non-Proton) sockets may also be monitored, either solely
 * for event notification (read, write, and timer) or event
 * notification and use with pn_io_t interfaces.
 */
typedef struct pn_selector_t pn_selector_t;

pn_selector_t *pni_selector(void);
void pn_selector_free(pn_selector_t *selector);
void pn_selector_add(pn_selector_t *selector, pn_selectable_t *selectable);
void pn_selector_update(pn_selector_t *selector, pn_selectable_t *selectable);
void pn_selector_remove(pn_selector_t *selector, pn_selectable_t *selectable);
size_t pn_selector_size(pn_selector_t *selector);
int pn_selector_select(pn_selector_t *select, int timeout);
pn_selectable_t *pn_selector_next(pn_selector_t *select, int *events);

#endif /* selector.h */
