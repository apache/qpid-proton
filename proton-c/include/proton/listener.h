#ifndef PROTON_LISTENER_H
#define PROTON_LISTENER_H

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

#include <proton/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 *
 * Listener API for the proton @proactor
 *
 * @defgroup listener Listener
 * @ingroup proactor
 * @{
 */

typedef struct pn_proactor_t pn_proactor_t;
typedef struct pn_condition_t pn_condition_t;

/**
 * Listener accepts connections, see pn_proactor_listen()
 */
typedef struct pn_listener_t pn_listener_t;

/**
 * The proactor that created the listener.
 */
pn_proactor_t *pn_listener_proactor(pn_listener_t *c);

/**
 * Get the error condition for a listener.
 */
pn_condition_t *pn_listener_condition(pn_listener_t *l);

/**
 * Get the user-provided value associated with the listener in pn_proactor_listen()
 * The start address is aligned so you can cast it to any type.
 */
pn_rwbytes_t pn_listener_get_extra(pn_listener_t*);

/**
 * Close the listener (thread safe).
 */
void pn_listener_close(pn_listener_t *l);

/**
 *@}
 */

#ifdef __cplusplus
}
#endif

#endif // PROTON_LISTENER_H
