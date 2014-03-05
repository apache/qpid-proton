#ifndef PROTON_DISPOSITION_H
#define PROTON_DISPOSITION_H 1

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
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @file
 * Disposition API for the proton Engine.
 *
 * @defgroup disposition Disposition
 * @ingroup delivery
 * @{
 */

typedef struct pn_disposition_t pn_disposition_t;

/**
 * The state/outcome of a message transfer.
 *
 * @todo document each value
 */

#define PN_RECEIVED (0x0000000000000023)
#define PN_ACCEPTED (0x0000000000000024)
#define PN_REJECTED (0x0000000000000025)
#define PN_RELEASED (0x0000000000000026)
#define PN_MODIFIED (0x0000000000000027)

PN_EXTERN uint64_t pn_disposition_type(pn_disposition_t *disposition);
PN_EXTERN pn_condition_t *pn_disposition_condition(pn_disposition_t *disposition);
PN_EXTERN pn_data_t *pn_disposition_data(pn_disposition_t *disposition);
PN_EXTERN uint32_t pn_disposition_get_section_number(pn_disposition_t *disposition);
PN_EXTERN void pn_disposition_set_section_number(pn_disposition_t *disposition, uint32_t section_number);
PN_EXTERN uint64_t pn_disposition_get_section_offset(pn_disposition_t *disposition);
PN_EXTERN void pn_disposition_set_section_offset(pn_disposition_t *disposition, uint64_t section_offset);
PN_EXTERN bool pn_disposition_is_failed(pn_disposition_t *disposition);
PN_EXTERN void pn_disposition_set_failed(pn_disposition_t *disposition, bool failed);
PN_EXTERN bool pn_disposition_is_undeliverable(pn_disposition_t *disposition);
PN_EXTERN void pn_disposition_set_undeliverable(pn_disposition_t *disposition, bool undeliverable);
PN_EXTERN pn_data_t *pn_disposition_annotations(pn_disposition_t *disposition);

/** @}
 */

#ifdef __cplusplus
}
#endif

#endif /* disposition.h */
