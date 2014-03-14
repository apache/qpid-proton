#ifndef PROTON_SELECTABLE_H
#define PROTON_SELECTABLE_H 1

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

#include <proton/import_export.h>
#include <proton/object.h>
#include <proton/io.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef pn_iterator_t pn_selectables_t;
typedef struct pn_selectable_t pn_selectable_t;

PN_EXTERN pn_selectables_t *pn_selectables(void);
PN_EXTERN pn_selectable_t *pn_selectables_next(pn_selectables_t *selectables);
PN_EXTERN void pn_selectables_free(pn_selectables_t *selectables);

PN_EXTERN pn_socket_t pn_selectable_fd(pn_selectable_t *selectable);
PN_EXTERN ssize_t pn_selectable_capacity(pn_selectable_t *selectable);
PN_EXTERN ssize_t pn_selectable_pending(pn_selectable_t *selectable);
PN_EXTERN pn_timestamp_t pn_selectable_deadline(pn_selectable_t *selectable);
PN_EXTERN void pn_selectable_readable(pn_selectable_t *selectable);
PN_EXTERN void pn_selectable_writable(pn_selectable_t *selectable);
PN_EXTERN void pn_selectable_expired(pn_selectable_t *selectable);
PN_EXTERN bool pn_selectable_is_registered(pn_selectable_t *selectable);
PN_EXTERN void pn_selectable_set_registered(pn_selectable_t *selectable, bool registered);
PN_EXTERN bool pn_selectable_is_terminal(pn_selectable_t *selectable);
PN_EXTERN void pn_selectable_free(pn_selectable_t *selectable);

#ifdef __cplusplus
}
#endif

#endif /* selectable.h */
