#ifndef PROTON_CONDITION_H
#define PROTON_CONDITION_H 1

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
#include <proton/codec.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @file
 * Condition API for the proton Engine.
 *
 * @defgroup condition Condition
 * @ingroup connection
 * @{
 */

/**
 * Represents an AMQP Condition.
 */
typedef struct pn_condition_t pn_condition_t;

PN_EXTERN bool pn_condition_is_set(pn_condition_t *condition);
PN_EXTERN void pn_condition_clear(pn_condition_t *condition);

PN_EXTERN const char *pn_condition_get_name(pn_condition_t *condition);
PN_EXTERN int pn_condition_set_name(pn_condition_t *condition, const char *name);

PN_EXTERN const char *pn_condition_get_description(pn_condition_t *condition);
PN_EXTERN int pn_condition_set_description(pn_condition_t *condition, const char *description);

PN_EXTERN pn_data_t *pn_condition_info(pn_condition_t *condition);

PN_EXTERN bool pn_condition_is_redirect(pn_condition_t *condition);
PN_EXTERN const char *pn_condition_redirect_host(pn_condition_t *condition);
PN_EXTERN int pn_condition_redirect_port(pn_condition_t *condition);

/** @}
 */

#ifdef __cplusplus
}
#endif

#endif /* condition.h */
