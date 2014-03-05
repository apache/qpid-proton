#ifndef PROTON_DELIVERY_H
#define PROTON_DELIVERY_H 1

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
#include <proton/disposition.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @file
 * Delivery API for the proton Engine.
 *
 * @defgroup delivery Delivery
 * @ingroup engine
 * @{
 */

typedef struct pn_delivery_tag_t {
  size_t size;
  const char *bytes;
} pn_delivery_tag_t;

#ifndef SWIG  // older versions of SWIG choke on this:
static inline pn_delivery_tag_t pn_dtag(const char *bytes, size_t size) {
  pn_delivery_tag_t dtag = {size, bytes};
  return dtag;
}
#endif

PN_EXTERN pn_delivery_t *pn_delivery(pn_link_t *link, pn_delivery_tag_t tag);
PN_EXTERN pn_delivery_tag_t pn_delivery_tag(pn_delivery_t *delivery);
PN_EXTERN pn_link_t *pn_delivery_link(pn_delivery_t *delivery);
// how do we do delivery state?
PN_EXTERN pn_disposition_t *pn_delivery_local(pn_delivery_t *delivery);
PN_EXTERN uint64_t pn_delivery_local_state(pn_delivery_t *delivery);
PN_EXTERN pn_disposition_t *pn_delivery_remote(pn_delivery_t *delivery);
PN_EXTERN uint64_t pn_delivery_remote_state(pn_delivery_t *delivery);
PN_EXTERN bool pn_delivery_settled(pn_delivery_t *delivery);
PN_EXTERN size_t pn_delivery_pending(pn_delivery_t *delivery);
PN_EXTERN bool pn_delivery_partial(pn_delivery_t *delivery);
PN_EXTERN bool pn_delivery_writable(pn_delivery_t *delivery);
PN_EXTERN bool pn_delivery_readable(pn_delivery_t *delivery);
PN_EXTERN bool pn_delivery_updated(pn_delivery_t *delivery);
PN_EXTERN void pn_delivery_update(pn_delivery_t *delivery, uint64_t state);
PN_EXTERN void pn_delivery_clear(pn_delivery_t *delivery);
//int pn_delivery_format(pn_delivery_t *delivery);
PN_EXTERN void pn_delivery_settle(pn_delivery_t *delivery);
PN_EXTERN void pn_delivery_dump(pn_delivery_t *delivery);
PN_EXTERN void *pn_delivery_get_context(pn_delivery_t *delivery);
PN_EXTERN void pn_delivery_set_context(pn_delivery_t *delivery, void *context);
PN_EXTERN bool pn_delivery_buffered(pn_delivery_t *delivery);

/** Extracts the first delivery on the connection that has pending
 *  operations.
 *
 * Retrieves the first delivery on the Connection that has pending
 * operations. A readable delivery indicates message data is waiting
 * to be read. A writable delivery indicates that message data may be
 * sent. An updated delivery indicates that the delivery's disposition
 * has changed. A delivery will never be both readable and writible,
 * but it may be both readable and updated or both writiable and
 * updated.
 *
 * @param[in] connection the connection
 * @return the first delivery object that needs to be serviced, else
 * NULL if none
 */
PN_EXTERN pn_delivery_t *pn_work_head(pn_connection_t *connection);

/** Get the next delivery on the connection that needs has pending
 *  operations.
 *
 * @param[in] delivery the previous delivery retrieved from
 *                     either pn_work_head() or pn_work_next()
 * @return the next delivery that has pending operations, else
 * NULL if none
 */
PN_EXTERN pn_delivery_t *pn_work_next(pn_delivery_t *delivery);

/** @}
 */

#ifdef __cplusplus
}
#endif

#endif /* delivery.h */
