#ifndef PROTON_CONNECTION_H
#define PROTON_CONNECTION_H 1

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
 * Connection API for the proton Engine.
 *
 * @defgroup connection Connection
 * @ingroup engine
 * @{
 */

#define PN_LOCAL_UNINIT (1)    /**< local endpoint requires initialization */
#define PN_LOCAL_ACTIVE (2)    /**< local endpoint is active */
#define PN_LOCAL_CLOSED (4)    /**< local endpoint is closed */
#define PN_REMOTE_UNINIT (8)   /**< remote endpoint pending initialization by peer */
#define PN_REMOTE_ACTIVE (16)  /**< remote endpoint is active */
#define PN_REMOTE_CLOSED (32)  /**< remote endpoint has closed */

#define PN_LOCAL_MASK (PN_LOCAL_UNINIT | PN_LOCAL_ACTIVE | PN_LOCAL_CLOSED)
#define PN_REMOTE_MASK (PN_REMOTE_UNINIT | PN_REMOTE_ACTIVE | PN_REMOTE_CLOSED)

/** Factory to construct a new Connection.
 *
 * @return pointer to a new connection object.
 */
PN_EXTERN pn_connection_t *pn_connection(void);

PN_EXTERN void pn_connection_collect(pn_connection_t *connection, pn_collector_t *collector);

/** Retrieve the state of the connection.
 *
 * @param[in] connection the connection
 * @return the connection's state flags
 */
PN_EXTERN pn_state_t pn_connection_state(pn_connection_t *connection);
/** @todo: needs documentation */
PN_EXTERN pn_error_t *pn_connection_error(pn_connection_t *connection);
PN_EXTERN pn_condition_t *pn_connection_condition(pn_connection_t *connection);
PN_EXTERN pn_condition_t *pn_connection_remote_condition(pn_connection_t *connection);
/** @todo: needs documentation */
PN_EXTERN const char *pn_connection_get_container(pn_connection_t *connection);
/** @todo: needs documentation */
PN_EXTERN void pn_connection_set_container(pn_connection_t *connection, const char *container);
/** @todo: needs documentation */
PN_EXTERN const char *pn_connection_get_hostname(pn_connection_t *connection);
/** @todo: needs documentation */
PN_EXTERN void pn_connection_set_hostname(pn_connection_t *connection, const char *hostname);
PN_EXTERN const char *pn_connection_remote_container(pn_connection_t *connection);
PN_EXTERN const char *pn_connection_remote_hostname(pn_connection_t *connection);
PN_EXTERN pn_data_t *pn_connection_offered_capabilities(pn_connection_t *connection);
PN_EXTERN pn_data_t *pn_connection_desired_capabilities(pn_connection_t *connection);
PN_EXTERN pn_data_t *pn_connection_properties(pn_connection_t *connection);
PN_EXTERN pn_data_t *pn_connection_remote_offered_capabilities(pn_connection_t *connection);
PN_EXTERN pn_data_t *pn_connection_remote_desired_capabilities(pn_connection_t *connection);
PN_EXTERN pn_data_t *pn_connection_remote_properties(pn_connection_t *connection);
PN_EXTERN void pn_connection_reset(pn_connection_t *connection);
PN_EXTERN void pn_connection_open(pn_connection_t *connection);
PN_EXTERN void pn_connection_close(pn_connection_t *connection);
PN_EXTERN void pn_connection_free(pn_connection_t *connection);

/** Access the application context that is associated with the
 *  connection.
 *
 * @param[in] connection the connection whose context is to be returned.
 *
 * @return the application context that was passed to pn_connection_set_context()
 */
PN_EXTERN void *pn_connection_get_context(pn_connection_t *connection);

/** Assign a new application context to the connection.
 *
 * @param[in] connection the connection which will hold the context.
 * @param[in] context new application context to associate with the
 *                    connection
 */
PN_EXTERN void pn_connection_set_context(pn_connection_t *connection, void *context);

PN_EXTERN pn_transport_t *pn_connection_transport(pn_connection_t *connection);

/** @}
 */

#ifdef __cplusplus
}
#endif

#endif /* connection.h */
