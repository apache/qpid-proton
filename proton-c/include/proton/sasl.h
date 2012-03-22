#ifndef _PROTON_SASL_H
#define _PROTON_SASL_H 1

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

#include <sys/types.h>
#include <stdbool.h>
#include <proton/value.h>

typedef struct pn_sasl_t pn_sasl_t;

typedef enum {SASL_NONE=-1, SASL_OK=0, SASL_AUTH=1, SASL_SYS=2, SASL_PERM=3,
              SASL_TEMP=4} pn_sasl_outcome_t;

pn_sasl_t *pn_sasl();
void pn_sasl_client(pn_sasl_t *sasl, const char *mechanism, const char *username, const char *password);
void pn_sasl_server(pn_sasl_t *sasl);
void pn_sasl_auth(pn_sasl_t *sasl, pn_sasl_outcome_t outcome);
bool pn_sasl_init(pn_sasl_t *sasl);
const char *pn_sasl_mechanism(pn_sasl_t *sasl);
pn_binary_t *pn_sasl_challenge(pn_sasl_t *sasl);
pn_binary_t *pn_sasl_response(pn_sasl_t *sasl);
pn_sasl_outcome_t pn_sasl_outcome(pn_sasl_t *sasl);
ssize_t pn_sasl_input(pn_sasl_t *sasl, char *bytes, size_t available);
ssize_t pn_sasl_output(pn_sasl_t *sasl, char *bytes, size_t size);
void pn_sasl_trace(pn_sasl_t *sasl, pn_trace_t trace);
void pn_sasl_destroy(pn_sasl_t *sasl);

#endif /* sasl.h */
