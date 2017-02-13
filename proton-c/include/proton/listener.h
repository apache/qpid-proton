#ifndef PROTON_LISTENER_H
#define PROTON_LISTENER_H 1

/*
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
 */

#include <proton/condition.h>
#include <proton/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 *
 * **Experimental** - A listener for incoming connections for the @ref
 * proactor.
 *
 * @addtogroup proactor
 * @{
 */

/**
 * Create a listener.
 *
 * You can use pn_listener_set_context() or pn_listener_attachments() to set
 * application data that can be accessed when accepting connections.
 *
 * You must pass the returned listener to pn_proactor_listen(), the proactor
 * will free the listener when it is no longer active.
 */
PNP_EXTERN pn_listener_t *pn_listener(void);

/**
 * Asynchronously accept a connection using the listener.
 *
 * @param[in] connection the listener takes ownership, do not free.
 */
PNP_EXTERN int pn_listener_accept(pn_listener_t*, pn_connection_t *connection);

/**
 * Get the error condition for a listener.
 */
PNP_EXTERN pn_condition_t *pn_listener_condition(pn_listener_t *l);

/**
 * @cond INTERNAL
 */

/**
 * @deprecated
 *
 * Get the application context that is associated with a listener.
 */
PNP_EXTERN void *pn_listener_get_context(pn_listener_t *listener);

/**
 * @deprecated
 *
 * Set a new application context for a listener.
 */
PNP_EXTERN void pn_listener_set_context(pn_listener_t *listener, void *context);

/**
 * @endcond
 */

/**
 * Get the attachments that are associated with a listener object.
 */
PNP_EXTERN pn_record_t *pn_listener_attachments(pn_listener_t *listener);

/**
 * Close the listener (thread safe).
 */
PNP_EXTERN void pn_listener_close(pn_listener_t *l);

/**
 * The proactor associated with a listener.
 */
PNP_EXTERN pn_proactor_t *pn_listener_proactor(pn_listener_t *c);


/**
 *@}
 */

#ifdef __cplusplus
}
#endif

#endif /* listener.h */
