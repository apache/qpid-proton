#ifndef PROTON_REACTOR_H
#define PROTON_REACTOR_H 1

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
#include <proton/type_compat.h>
#include <proton/event.h>
#include <proton/selectable.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 *
 * Reactor API for proton.
 *
 * @defgroup reactor Reactor
 * @ingroup reactor
 * @{
 */

typedef struct pn_handler_t pn_handler_t;
typedef struct pn_reactor_t pn_reactor_t;
typedef struct pn_acceptor_t pn_acceptor_t;
typedef struct pn_timer_t pn_timer_t;
typedef struct pn_task_t pn_task_t;

PN_EXTERN pn_handler_t *pn_handler(void (*dispatch)(pn_handler_t *, pn_event_t *));
PN_EXTERN pn_handler_t *pn_handler_new(void (*dispatch)(pn_handler_t *, pn_event_t *), size_t size,
                                       void (*finalize)(pn_handler_t *));
PN_EXTERN void pn_handler_free(pn_handler_t *handler);
PN_EXTERN void *pn_handler_mem(pn_handler_t *handler);
PN_EXTERN void pn_handler_add(pn_handler_t *handler, pn_handler_t *child);
PN_EXTERN void pn_handler_dispatch(pn_handler_t *handler, pn_event_t *event);

PN_EXTERN pn_reactor_t *pn_reactor(void);
PN_EXTERN pn_record_t *pn_reactor_attachments(pn_reactor_t *reactor);
PN_EXTERN void pn_reactor_free(pn_reactor_t *reactor);
PN_EXTERN pn_collector_t *pn_reactor_collector(pn_reactor_t *reactor);
PN_EXTERN pn_handler_t *pn_reactor_handler(pn_reactor_t *reactor);
PN_EXTERN pn_io_t *pn_reactor_io(pn_reactor_t *reactor);
PN_EXTERN pn_selector_t *pn_reactor_selector(pn_reactor_t *reactor);
PN_EXTERN pn_list_t *pn_reactor_children(pn_reactor_t *reactor);
PN_EXTERN pn_selectable_t *pn_reactor_selectable(pn_reactor_t *reactor);
PN_EXTERN void pn_reactor_update(pn_reactor_t *reactor, pn_selectable_t *selectable);
PN_EXTERN pn_acceptor_t *pn_reactor_acceptor(pn_reactor_t *reactor, const char *host, const char *port,
                                             pn_handler_t *handler);
PN_EXTERN pn_connection_t *pn_reactor_connection(pn_reactor_t *reactor, pn_handler_t *handler);
PN_EXTERN void pn_reactor_start(pn_reactor_t *reactor);
PN_EXTERN bool pn_reactor_work(pn_reactor_t *reactor, int timeout);
PN_EXTERN void pn_reactor_stop(pn_reactor_t *reactor);
PN_EXTERN void pn_reactor_run(pn_reactor_t *reactor);
PN_EXTERN pn_task_t *pn_reactor_schedule(pn_reactor_t *reactor, int delay, pn_handler_t *handler);


PN_EXTERN void pn_acceptor_close(pn_reactor_t *reactor, pn_acceptor_t *acceptor);

PN_EXTERN pn_timer_t *pn_timer(pn_collector_t *collector);
PN_EXTERN pn_timestamp_t pn_timer_deadline(pn_timer_t *timer);
PN_EXTERN void pn_timer_tick(pn_timer_t *timer, pn_timestamp_t now);
PN_EXTERN pn_task_t *pn_timer_schedule(pn_timer_t *timer, pn_timestamp_t deadline);
PN_EXTERN int pn_timer_tasks(pn_timer_t *timer);

PN_EXTERN pn_record_t *pn_task_attachments(pn_task_t *task);

PN_EXTERN pn_reactor_t *pn_event_reactor(pn_event_t *event);

/** @}
 */

#ifdef __cplusplus
}
#endif

#endif /* reactor.h */
