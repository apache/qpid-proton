#ifndef PROTON_TERMINUS_H
#define PROTON_TERMINUS_H 1

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
 * Terminus API for the proton Engine.
 *
 * @defgroup terminus Terminus
 * @ingroup link
 * @{
 */

/**
 * Encapsulates the endpoint state associated with an AMQP Terminus.
 */
typedef struct pn_terminus_t pn_terminus_t;

typedef enum {
  PN_UNSPECIFIED = 0,
  PN_SOURCE = 1,
  PN_TARGET = 2,
  PN_COORDINATOR = 3
} pn_terminus_type_t;
typedef enum {
  PN_NONDURABLE = 0,
  PN_CONFIGURATION = 1,
  PN_DELIVERIES = 2
} pn_durability_t;
typedef enum {
  PN_LINK_CLOSE,
  PN_SESSION_CLOSE,
  PN_CONNECTION_CLOSE,
  PN_NEVER
} pn_expiry_policy_t;
typedef enum {
  PN_DIST_MODE_UNSPECIFIED,
  PN_DIST_MODE_COPY,
  PN_DIST_MODE_MOVE
} pn_distribution_mode_t;
typedef enum {
  PN_SND_UNSETTLED = 0,
  PN_SND_SETTLED = 1,
  PN_SND_MIXED = 2
} pn_snd_settle_mode_t;
typedef enum {
  PN_RCV_FIRST = 0,  /**< implicitly settle rcvd xfers */
  PN_RCV_SECOND = 1  /**< explicit disposition required */
} pn_rcv_settle_mode_t;

PN_EXTERN pn_terminus_type_t pn_terminus_get_type(pn_terminus_t *terminus);
PN_EXTERN int pn_terminus_set_type(pn_terminus_t *terminus, pn_terminus_type_t type);

PN_EXTERN const char *pn_terminus_get_address(pn_terminus_t *terminus);
PN_EXTERN int pn_terminus_set_address(pn_terminus_t *terminus, const char *address);
PN_EXTERN pn_durability_t pn_terminus_get_durability(pn_terminus_t *terminus);
PN_EXTERN int pn_terminus_set_durability(pn_terminus_t *terminus,
                               pn_durability_t durability);
PN_EXTERN pn_expiry_policy_t pn_terminus_get_expiry_policy(pn_terminus_t *terminus);
PN_EXTERN int pn_terminus_set_expiry_policy(pn_terminus_t *terminus, pn_expiry_policy_t policy);
PN_EXTERN pn_seconds_t pn_terminus_get_timeout(pn_terminus_t *terminus);
PN_EXTERN int pn_terminus_set_timeout(pn_terminus_t *terminus, pn_seconds_t);
PN_EXTERN bool pn_terminus_is_dynamic(pn_terminus_t *terminus);
PN_EXTERN int pn_terminus_set_dynamic(pn_terminus_t *terminus, bool dynamic);
PN_EXTERN pn_data_t *pn_terminus_properties(pn_terminus_t *terminus);
PN_EXTERN pn_data_t *pn_terminus_capabilities(pn_terminus_t *terminus);
PN_EXTERN pn_data_t *pn_terminus_outcomes(pn_terminus_t *terminus);
PN_EXTERN pn_data_t *pn_terminus_filter(pn_terminus_t *terminus);
PN_EXTERN pn_distribution_mode_t pn_terminus_get_distribution_mode(const pn_terminus_t *terminus);
PN_EXTERN int pn_terminus_set_distribution_mode(pn_terminus_t *terminus, pn_distribution_mode_t m);
PN_EXTERN int pn_terminus_copy(pn_terminus_t *terminus, pn_terminus_t *src);

/** @}
 */

#ifdef __cplusplus
}
#endif

#endif /* terminus.h */
