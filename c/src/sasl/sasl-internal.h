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

#include "core/buffer.h"
#include "core/engine-internal.h"

#include "proton/types.h"
#include "proton/sasl.h"
#include "proton/sasl_plugin.h"

extern const pnx_sasl_implementation default_sasl_impl;
extern const pnx_sasl_implementation * const cyrus_sasl_impl;

// SASL APIs used by transport code
void pn_sasl_free(pn_transport_t *transport);
void pni_sasl_set_user_password(pn_transport_t *transport, const char *user, const char *authzid, const char *password);
void pni_sasl_set_remote_hostname(pn_transport_t *transport, const char* fqdn);
void pni_sasl_set_external_security(pn_transport_t *transport, int ssf, const char *authid);

struct pni_sasl_t {
  void *impl_context;
  const pnx_sasl_implementation* impl;
  char *selected_mechanism;
  char *included_mechanisms;
  const char *username;
  const char *authzid;
  char *password;
  const char *remote_fqdn;
  char *local_fqdn;
  char *external_auth;
  int external_ssf;
  size_t max_encrypt_size;
  pn_buffer_t* decoded_buffer;
  pn_buffer_t* encoded_buffer;
  pn_bytes_t bytes_out;
  pn_sasl_outcome_t outcome;
  enum pnx_sasl_state desired_state;
  enum pnx_sasl_state last_state;
  bool allow_insecure_mechs;
  bool client;
};

#endif /* sasl-internal.h */
