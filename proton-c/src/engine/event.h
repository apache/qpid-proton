#ifndef _PROTON_EVENT_H
#define _PROTON_EVENT_H 1

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

pn_event_t *pn_collector_put(pn_collector_t *collector, pn_event_type_t type);

void pn_event_init_transport(pn_event_t *event, pn_transport_t *transport);
void pn_event_init_connection(pn_event_t *event, pn_connection_t *connection);
void pn_event_init_session(pn_event_t *event, pn_session_t *session);
void pn_event_init_link(pn_event_t *event, pn_link_t *link);
void pn_event_init_delivery(pn_event_t *event, pn_delivery_t *delivery);

#endif /* event.h */
