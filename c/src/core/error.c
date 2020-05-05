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

#include <proton/error.h>
#include <proton/object.h>

#include "memory.h"
#include "platform/platform.h"
#include "util.h"

#include <proton/connection.h>
#include <proton/link.h>
#include <proton/session.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

struct pn_error_t {
  char *text;
  int code;
};

PN_STRUCT_CLASSDEF(pn_error)

pn_error_t *pn_error()
{
  pn_error_t *error = (pn_error_t *) pni_mem_allocate(PN_CLASSCLASS(pn_error), sizeof(pn_error_t));
  if (error != NULL) {
    error->code = 0;
    error->text = NULL;
  }
  return error;
}

void pn_error_free(pn_error_t *error)
{
  if (error) {
    pni_mem_subdeallocate(PN_CLASSCLASS(pn_error), error, error->text);
    pni_mem_deallocate(PN_CLASSCLASS(pn_error), error);
  }
}

void pn_error_clear(pn_error_t *error)
{
  if (error) {
    error->code = 0;
    pni_mem_subdeallocate(PN_CLASSCLASS(pn_error), error, error->text);
    error->text = NULL;
  }
}

int pn_error_set(pn_error_t *error, int code, const char *text)
{
  assert(error);
  pn_error_clear(error);
  if (code) {
    error->code = code;
    error->text = pn_strdup(text);
  }
  return code;
}

int pn_error_vformat(pn_error_t *error, int code, const char *fmt, va_list ap)
{
  assert(error);
  char text[1024];
  int n = vsnprintf(text, 1024, fmt, ap);
  if (n >= 1024) {
    text[1023] = '\0';
  }
  return pn_error_set(error, code, text);
}

int pn_error_format(pn_error_t *error, int code, const char *fmt, ...)
{
  assert(error);
  va_list ap;
  va_start(ap, fmt);
  int rcode = pn_error_vformat(error, code, fmt, ap);
  va_end(ap);
  return rcode;
}

int pn_error_code(pn_error_t *error)
{
  assert(error);
  return error->code;
}

const char *pn_error_text(pn_error_t *error)
{
  assert(error);
  return error->text;
}

int pn_error_copy(pn_error_t *error, pn_error_t *src)
{
  assert(error);
  if (src) {
    return pn_error_set(error, pn_error_code(src), pn_error_text(src));
  } else {
    pn_error_clear(error);
    return 0;
  }
}

const char *pn_code(int code)
{
  switch (code)
  {
  case 0: return "<ok>";
  case PN_EOS: return "PN_EOS";
  case PN_ERR: return "PN_ERR";
  case PN_OVERFLOW: return "PN_OVERFLOW";
  case PN_UNDERFLOW: return "PN_UNDERFLOW";
  case PN_STATE_ERR: return "PN_STATE_ERR";
  case PN_ARG_ERR: return "PN_ARG_ERR";
  case PN_TIMEOUT: return "PN_TIMEOUT";
  case PN_INTR: return "PN_INTR";
  case PN_INPROGRESS: return "PN_INPROGRESS";
  case PN_OUT_OF_MEMORY: return "PN_OUT_OF_MEMORY";
  case PN_ABORTED: return "PN_ABORTED";
  default: return "<unknown>";
  }
}

// Deprecated ABI compatibility stubs

// Constant to make sure that no one tries to change this.
static const pn_error_t pn_error_null = {NULL, 0};

pn_error_t *pn_connection_error(pn_connection_t *c) {return (pn_error_t*) &pn_error_null;}
pn_error_t *pn_session_error(pn_session_t *c) {return (pn_error_t*) &pn_error_null;}
pn_error_t *pn_link_error(pn_link_t *c) {return (pn_error_t*) &pn_error_null;}
