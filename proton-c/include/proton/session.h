#ifndef PROTON_SESSION_H
#define PROTON_SESSION_H 1

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
 * Session API for the proton Engine.
 *
 * @defgroup session Session
 * @ingroup engine
 * @{
 */

PN_EXTERN pn_state_t pn_session_state(pn_session_t *session);
PN_EXTERN pn_error_t *pn_session_error(pn_session_t *session);
PN_EXTERN pn_condition_t *pn_session_condition(pn_session_t *session);
PN_EXTERN pn_condition_t *pn_session_remote_condition(pn_session_t *session);
PN_EXTERN pn_connection_t *pn_session_connection(pn_session_t *session);
PN_EXTERN size_t pn_session_get_incoming_capacity(pn_session_t *ssn);
PN_EXTERN void pn_session_set_incoming_capacity(pn_session_t *ssn, size_t capacity);
PN_EXTERN size_t pn_session_outgoing_bytes(pn_session_t *ssn);
PN_EXTERN size_t pn_session_incoming_bytes(pn_session_t *ssn);
PN_EXTERN void pn_session_open(pn_session_t *session);
PN_EXTERN void pn_session_close(pn_session_t *session);
PN_EXTERN void pn_session_free(pn_session_t *session);
PN_EXTERN void *pn_session_get_context(pn_session_t *session);
PN_EXTERN void pn_session_set_context(pn_session_t *session, void *context);

/** Factory for creating a new session on the connection.
 *
 * A new session is created for the connection, and is added to the
 * set of sessions maintained by the connection.
 *
 * @param[in] connection the session will exist over this connection
 * @return pointer to new session
 */
PN_EXTERN pn_session_t *pn_session(pn_connection_t *connection);

/** Retrieve the first Session that matches the given state mask.
 *
 * Examines the state of each session owned by the connection, and
 * returns the first Session that matches the given state mask. If
 * state contains both local and remote flags, then an exact match
 * against those flags is performed. If state contains only local or
 * only remote flags, then a match occurs if any of the local or
 * remote flags are set respectively.
 *
 * @param[in] connection to be searched for matching sessions
 * @param[in] state mask to match
 * @return the first session owned by the connection that matches the
 * mask, else NULL if no sessions match
 */
PN_EXTERN pn_session_t *pn_session_head(pn_connection_t *connection, pn_state_t state);

/** Retrieve the next Session that matches the given state mask.
 *
 * When used with pn_session_head(), application can access all
 * Sessions on the connection that match the given state. See
 * pn_session_head() for description of match behavior.
 *
 * @param[in] session the previous session obtained from
 *                    pn_session_head() or pn_session_next()
 * @param[in] state mask to match.
 * @return the next session owned by the connection that matches the
 * mask, else NULL if no sessions match
 */
PN_EXTERN pn_session_t *pn_session_next(pn_session_t *session, pn_state_t state);

/** @}
 */

#ifdef __cplusplus
}
#endif

#endif /* session.h */
