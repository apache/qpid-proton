#ifndef PROACTOR_PROACTOR_INTERNAL_H
#define PROACTOR_PROACTOR_INTERNAL_H

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

#include <proton/type_compat.h>
#include <proton/import_export.h>
#include <proton/condition.h>

/* NOTE PNP_EXTERN is for use by proton-internal tests  */

/**
 * Parse a pn_proactor_addr string, copy data into buf as necessary.
 * Set *host and *port to point to the host and port strings.
 *
 * If the port is empty, replace it with "5672", if it is "amqp" or "amqps"
 * replace it with the numeric port value.
 *
 * @return 0 on success, PN_OVERFLOW if buf is too small.
 */
PNP_EXTERN int pni_parse_addr(const char *addr, char *buf, size_t len, const char **host, const char **port);

/**
 * Condition name for error conditions related to proton-IO.
 */
extern const char *PNI_IO_CONDITION;

/**
 * Format a proactor error condition with message "<what> (<host>:<port>): <msg>"
 */
void pni_proactor_set_cond(
  pn_condition_t *cond, const char *what, const char *host, const char *port, const char *msg);

#endif  /*!PROACTOR_PROACTOR_INTERNAL_H*/
