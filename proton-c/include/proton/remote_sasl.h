#ifndef PROTON_REMOTE_SASL_H
#define PROTON_REMOTE_SASL_H 1

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

#include <proton/import_export.h>
#include <proton/types.h>

#ifdef __cplusplus
extern "C" {
#endif

PN_EXPORT void pn_use_remote_authentication_service(pn_transport_t* transport, const char* address);
PN_EXPORT bool pn_is_authentication_service_connection(pn_connection_t* conn);
PN_EXPORT void pn_handle_authentication_service_connection_event(pn_event_t *e);

#ifdef __cplusplus
}
#endif

#endif /* remote_sasl.h */
