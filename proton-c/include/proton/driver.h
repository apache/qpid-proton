#ifndef _PROTON_DRIVER_H
#define _PROTON_DRIVER_H 1

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

#include <proton/engine.h>
#include <proton/sasl.h>
#include <stdlib.h>

typedef struct pn_driver_t pn_driver_t;
typedef struct pn_selectable_t pn_selectable_t;
typedef void (pn_callback_t)(pn_selectable_t *);

#define PN_SEL_RD (0x0001)
#define PN_SEL_WR (0x0002)

pn_driver_t *pn_driver(void);
void pn_driver_trace(pn_driver_t *d, pn_trace_t trace);
void pn_driver_run(pn_driver_t *d);
void pn_driver_stop(pn_driver_t *d);
void pn_driver_destroy(pn_driver_t *d);

pn_selectable_t *pn_acceptor(pn_driver_t *driver, char *host, char *port, pn_callback_t *callback, void* context);
pn_selectable_t *pn_connector(pn_driver_t *driver, char *host, char *port, pn_callback_t *callback, void* context);
void pn_selectable_trace(pn_selectable_t *sel, pn_trace_t trace);
pn_sasl_t *pn_selectable_sasl(pn_selectable_t *sel);
pn_connection_t *pn_selectable_connection(pn_selectable_t *sel);
void *pn_selectable_context(pn_selectable_t *sel);
void pn_selectable_destroy(pn_selectable_t *sel);

#endif /* driver.h */
