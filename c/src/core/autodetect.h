#ifndef PROTON_AUTODETECT_H
#define PROTON_AUTODETECT_H 1

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

#include "proton/types.h"

typedef enum {
  PNI_PROTOCOL_INSUFFICIENT,
  PNI_PROTOCOL_UNKNOWN,
  PNI_PROTOCOL_SSL,
  PNI_PROTOCOL_AMQP_SSL,
  PNI_PROTOCOL_AMQP_SASL,
  PNI_PROTOCOL_AMQP1,
  PNI_PROTOCOL_AMQP_OTHER
} pni_protocol_type_t;

#if __cplusplus
extern "C" {
#endif

pni_protocol_type_t pni_sniff_header(const char *data, size_t len);
const char* pni_protocol_name(pni_protocol_type_t p);

#if __cplusplus
}
#endif

#endif
