#ifndef _PROTON_SASL_INTERNAL_H
#define _PROTON_SASL_INTERNAL_H 1

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

#include <proton/sasl.h>
#include "../dispatcher/dispatcher.h"

#define SCRATCH (1024)

struct pn_sasl_t {
  pn_dispatcher_t *disp;
  bool init;
  pn_symbol_t *mechanism;
  pn_binary_t *challenge;
  pn_binary_t *response;
  pn_sasl_outcome_t outcome;
  char scratch[SCRATCH];
};

#endif /* sasl-internal.h */
