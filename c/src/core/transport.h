#ifndef _PROTON_TRANSPORT_INTERNAL_H
#define _PROTON_TRANSPORT_INTERNAL_H 1

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

void pn_delivery_map_init(pn_delivery_map_t *db, pn_sequence_t next);
void pn_delivery_map_del(pn_delivery_map_t *db, pn_delivery_t *delivery);
void pn_delivery_map_free(pn_delivery_map_t *db);
void pn_unmap_handle(pn_session_t *ssn, pn_link_t *link);
void pn_unmap_channel(pn_transport_t *transport, pn_session_t *ssn);

#endif /* transport.h */
