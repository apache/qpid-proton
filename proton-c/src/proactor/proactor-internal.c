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


/* Common platform-independent implementation for proactor libraries */

#include "proactor-internal.h"
#include <proton/error.h>
#include <proton/proactor.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static const char *AMQP_PORT = "5672";
static const char *AMQP_PORT_NAME = "amqp";
static const char *AMQPS_PORT = "5671";
static const char *AMQPS_PORT_NAME = "amqps";

const char *PNI_IO_CONDITION = "proton:io";

#ifndef _WIN32
/*
 * Common implementation for C99-friendly compilers.  Windows is
 * not and implements its own.
 */
int pn_proactor_addr(char *buf, size_t len, const char *host, const char *port) {
  return snprintf(buf, len, "%s:%s", host ? host : "", port ? port : "");
}
#endif

int pni_parse_addr(const char *addr, char *buf, size_t len, const char **host, const char **port)
{
  size_t hplen = strlen(addr);
  if (hplen >= len) {
    return PN_OVERFLOW;
  }
  memcpy(buf, addr, hplen+1);
  char *p = strrchr(buf, ':');
  if (p) {
    *port = p + 1;
    *p = '\0';
    if (**port == '\0' || !strcmp(*port, AMQP_PORT_NAME)) {
      *port = AMQP_PORT;
    } else if (!strcmp(*port, AMQPS_PORT_NAME)) {
      *port = AMQPS_PORT;
    }
  } else {
    *port = AMQP_PORT;
  }
  if (*buf) {
    *host = buf;
  } else {
    *host = NULL;
  }
  return 0;
}

static inline const char *nonull(const char *str) { return str ? str : ""; }

void pni_proactor_set_cond(
  pn_condition_t *cond, const char *what, const char *host, const char *port, const char *msg)
{
  if (!pn_condition_is_set(cond)) { /* Preserve older error information */
    pn_condition_format(cond, PNI_IO_CONDITION, "%s - %s %s:%s",
                        msg, what, nonull(host), nonull(port));
  }
}
