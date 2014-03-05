#ifndef PROTON_EVENT_H
#define PROTON_EVENT_H 1

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
 * Event API for the proton Engine.
 *
 * @defgroup event Event
 * @ingroup engine
 * @{
 */

/**
 * An event.
 */
typedef struct pn_event_t pn_event_t;

/**
 * An event type.
 */
typedef enum {
  PN_EVENT_NONE = 0,
  PN_CONNECTION_STATE = 1,
  PN_SESSION_STATE = 2,
  PN_LINK_STATE = 4,
  PN_LINK_FLOW = 8,
  PN_DELIVERY = 16,
  PN_TRANSPORT = 32
} pn_event_type_t;

PN_EXTERN const char *pn_event_type_name(pn_event_type_t type);

PN_EXTERN pn_collector_t *pn_collector(void);
PN_EXTERN void pn_collector_free(pn_collector_t *collector);
PN_EXTERN pn_event_t *pn_collector_peek(pn_collector_t *collector);
PN_EXTERN bool pn_collector_pop(pn_collector_t *collector);

PN_EXTERN pn_event_type_t pn_event_type(pn_event_t *event);
PN_EXTERN pn_connection_t *pn_event_connection(pn_event_t *event);
PN_EXTERN pn_session_t *pn_event_session(pn_event_t *event);
PN_EXTERN pn_link_t *pn_event_link(pn_event_t *event);
PN_EXTERN pn_delivery_t *pn_event_delivery(pn_event_t *event);
PN_EXTERN pn_transport_t *pn_event_transport(pn_event_t *event);

#ifdef __cplusplus
}
#endif

/** @}
 */

#endif /* event.h */
