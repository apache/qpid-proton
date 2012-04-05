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

#include <proton/errors.h>
#include <proton/engine.h>
#include <proton/sasl.h>

typedef struct pn_driver_t pn_driver_t;
typedef struct pn_listener_t pn_listener_t;
typedef struct pn_connector_t pn_connector_t;

pn_driver_t *pn_driver(void);
void pn_driver_trace(pn_driver_t *d, pn_trace_t trace);
void pn_driver_wakeup(pn_driver_t *d);
void pn_driver_wait(pn_driver_t *d);
pn_connector_t *pn_driver_listen(pn_driver_t *d);
pn_connector_t *pn_driver_process(pn_driver_t *d);
void pn_driver_destroy(pn_driver_t *d);

pn_listener_t *pn_listener(pn_driver_t *driver, const char *host,
                           const char *port, void* context);
pn_listener_t *pn_listener_fd(pn_driver_t *driver, int fd, void *context);
void pn_listener_trace(pn_listener_t *listener, pn_trace_t trace);
void *pn_listener_context(pn_listener_t *listener);
void pn_listener_close(pn_listener_t *listener);
void pn_listener_destroy(pn_listener_t *listener);

pn_connector_t *pn_connector(pn_driver_t *driver, const char *host,
                             const char *port, void* context);
pn_connector_t *pn_connector_fd(pn_driver_t *driver, int fd, void *context);
void pn_connector_trace(pn_connector_t *ctor, pn_trace_t trace);
pn_listener_t *pn_connector_listener(pn_connector_t *ctor);
pn_sasl_t *pn_connector_sasl(pn_connector_t *ctor);
pn_connection_t *pn_connector_connection(pn_connector_t *ctor);
void *pn_connector_context(pn_connector_t *ctor);
void pn_connector_set_context(pn_connector_t *ctor, void *context);
void pn_connector_eos(pn_connector_t *ctor);
void pn_connector_close(pn_connector_t *ctor);
bool pn_connector_closed(pn_connector_t *ctor);
void pn_connector_destroy(pn_connector_t *ctor);

#endif /* driver.h */
