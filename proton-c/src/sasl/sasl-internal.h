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

#ifndef PROTON_SASL_INTERNAL_H
#define PROTON_SASL_INTERNAL_H 1

#include "proton/types.h"

/** Destructor for the given SASL layer.
 *
 * @param[in] sasl the SASL object to free. No longer valid on
 *                 return.
 */
void pn_sasl_free(pn_transport_t *transport);

ssize_t pn_sasl_input(pn_transport_t *transport, const char *bytes, size_t available);
ssize_t pn_sasl_output(pn_transport_t *transport, char *bytes, size_t size);

void pni_sasl_set_user_password(pn_transport_t *transport, const char *user, const char *password);
void pni_sasl_set_remote_hostname(pn_transport_t *transport, const char* fqdn);

#endif /* sasl-internal.h */
