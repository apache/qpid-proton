#ifndef PROTON_LINK_H
#define PROTON_LINK_H 1

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
 * Link API for the proton Engine.
 *
 * @defgroup link Link
 * @ingroup engine
 * @{
 */

PN_EXTERN pn_link_t *pn_sender(pn_session_t *session, const char *name);
PN_EXTERN pn_link_t *pn_receiver(pn_session_t *session, const char *name);
PN_EXTERN const char *pn_link_name(pn_link_t *link);
PN_EXTERN bool pn_link_is_sender(pn_link_t *link);
PN_EXTERN bool pn_link_is_receiver(pn_link_t *link);
PN_EXTERN pn_state_t pn_link_state(pn_link_t *link);
PN_EXTERN pn_error_t *pn_link_error(pn_link_t *link);
PN_EXTERN pn_condition_t *pn_link_condition(pn_link_t *link);
PN_EXTERN pn_condition_t *pn_link_remote_condition(pn_link_t *link);
PN_EXTERN pn_session_t *pn_link_session(pn_link_t *link);
PN_EXTERN pn_terminus_t *pn_link_source(pn_link_t *link);
PN_EXTERN pn_terminus_t *pn_link_target(pn_link_t *link);
PN_EXTERN pn_terminus_t *pn_link_remote_source(pn_link_t *link);
PN_EXTERN pn_terminus_t *pn_link_remote_target(pn_link_t *link);
PN_EXTERN pn_delivery_t *pn_link_current(pn_link_t *link);
PN_EXTERN bool pn_link_advance(pn_link_t *link);
PN_EXTERN int pn_link_credit(pn_link_t *link);
PN_EXTERN int pn_link_queued(pn_link_t *link);
PN_EXTERN int pn_link_remote_credit(pn_link_t *link);
PN_EXTERN int pn_link_available(pn_link_t *link);
PN_EXTERN pn_snd_settle_mode_t pn_link_snd_settle_mode(pn_link_t *link);
PN_EXTERN pn_rcv_settle_mode_t pn_link_rcv_settle_mode(pn_link_t *link);
PN_EXTERN pn_snd_settle_mode_t pn_link_remote_snd_settle_mode(pn_link_t *link);
PN_EXTERN pn_rcv_settle_mode_t pn_link_remote_rcv_settle_mode(pn_link_t *link);
PN_EXTERN void pn_link_set_snd_settle_mode(pn_link_t *link, pn_snd_settle_mode_t);
PN_EXTERN void pn_link_set_rcv_settle_mode(pn_link_t *link, pn_rcv_settle_mode_t);

PN_EXTERN int pn_link_unsettled(pn_link_t *link);
PN_EXTERN pn_delivery_t *pn_unsettled_head(pn_link_t *link);
PN_EXTERN pn_delivery_t *pn_unsettled_next(pn_delivery_t *delivery);

PN_EXTERN void pn_link_open(pn_link_t *sender);
PN_EXTERN void pn_link_close(pn_link_t *sender);
PN_EXTERN void pn_link_free(pn_link_t *sender);
PN_EXTERN void *pn_link_get_context(pn_link_t *link);
PN_EXTERN void pn_link_set_context(pn_link_t *link, void *context);
PN_EXTERN bool pn_link_get_drain(pn_link_t *link);

/**
 * @defgroup sender Sender
 * @{
 */
PN_EXTERN void pn_link_offered(pn_link_t *sender, int credit);
PN_EXTERN ssize_t pn_link_send(pn_link_t *sender, const char *bytes, size_t n);
PN_EXTERN int pn_link_drained(pn_link_t *sender);
//PN_EXTERN void pn_link_abort(pn_sender_t *sender);
/** @} */

// receiver
/**
 * @defgroup receiver Receiver
 * @{
 */
PN_EXTERN void pn_link_flow(pn_link_t *receiver, int credit);
PN_EXTERN void pn_link_drain(pn_link_t *receiver, int credit);
PN_EXTERN void pn_link_set_drain(pn_link_t *receiver, bool drain);
PN_EXTERN ssize_t pn_link_recv(pn_link_t *receiver, char *bytes, size_t n);
PN_EXTERN bool pn_link_draining(pn_link_t *receiver);
/** @} */

/** Retrieve the first Link that matches the given state mask.
 *
 * Examines the state of each Link owned by the connection and returns
 * the first Link that matches the given state mask. If state contains
 * both local and remote flags, then an exact match against those
 * flags is performed. If state contains only local or only remote
 * flags, then a match occurs if any of the local or remote flags are
 * set respectively.
 *
 * @param[in] connection to be searched for matching Links
 * @param[in] state mask to match
 * @return the first Link owned by the connection that matches the
 * mask, else NULL if no Links match
 */
PN_EXTERN pn_link_t *pn_link_head(pn_connection_t *connection, pn_state_t state);

/** Retrieve the next Link that matches the given state mask.
 *
 * When used with pn_link_head(), the application can access all Links
 * on the connection that match the given state. See pn_link_head()
 * for description of match behavior.
 *
 * @param[in] link the previous Link obtained from pn_link_head() or
 *                 pn_link_next()
 * @param[in] state mask to match
 * @return the next session owned by the connection that matches the
 * mask, else NULL if no sessions match
 */
PN_EXTERN pn_link_t *pn_link_next(pn_link_t *link, pn_state_t state);

/** @}
 */

#ifdef __cplusplus
}
#endif

#endif /* link.h */

