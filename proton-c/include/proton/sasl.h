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

typedef struct pn_sasl_t pn_sasl_t;

typedef enum {
  PN_SASL_NONE=-1,
  PN_SASL_OK=0,
  PN_SASL_AUTH=1,
  PN_SASL_SYS=2,
  PN_SASL_PERM=3,
  PN_SASL_TEMP=4
} pn_sasl_outcome_t;

typedef enum {
  PN_SASL_CONF,
  PN_SASL_IDLE,
  PN_SASL_STEP,
  PN_SASL_PASS,
  PN_SASL_FAIL
} pn_sasl_state_t;

pn_sasl_t *pn_sasl();
pn_sasl_state_t pn_sasl_state(pn_sasl_t *sasl);
void pn_sasl_mechanisms(pn_sasl_t *sasl, const char *mechanisms);
const char *pn_sasl_remote_mechanisms(pn_sasl_t *sasl);
void pn_sasl_client(pn_sasl_t *sasl);
void pn_sasl_server(pn_sasl_t *sasl);
void pn_sasl_plain(pn_sasl_t *sasl, const char *username, const char *password);
size_t pn_sasl_pending(pn_sasl_t *sasl);
ssize_t pn_sasl_recv(pn_sasl_t *sasl, char *bytes, size_t size);
ssize_t pn_sasl_send(pn_sasl_t *sasl, const char *bytes, size_t size);
void pn_sasl_done(pn_sasl_t *sasl, pn_sasl_outcome_t outcome);
pn_sasl_outcome_t pn_sasl_outcome(pn_sasl_t *sasl);
ssize_t pn_sasl_input(pn_sasl_t *sasl, char *bytes, size_t available);
ssize_t pn_sasl_output(pn_sasl_t *sasl, char *bytes, size_t size);
void pn_sasl_trace(pn_sasl_t *sasl, pn_trace_t trace);
void pn_sasl_destroy(pn_sasl_t *sasl);


#endif /* sasl.h */
