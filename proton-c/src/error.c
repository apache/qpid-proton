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

#define _POSIX_C_SOURCE (200112)

#include <proton/error.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

struct pn_error_t {
  int code;
  char *text;
  pn_error_t *root;
};

pn_error_t *pn_error()
{
  pn_error_t *error = (pn_error_t *) malloc(sizeof(pn_error_t));
  error->code = 0;
  error->text = NULL;
  error->root = NULL;
  return error;
}

void pn_error_free(pn_error_t *error)
{
  if (error) {
    free(error->text);
    free(error);
  }
}

void pn_error_clear(pn_error_t *error)
{
  if (error) {
    error->code = 0;
    free(error->text);
    error->text = NULL;
    error->root = NULL;
  }
}

int pn_error_set(pn_error_t *error, int code, const char *text)
{
  pn_error_clear(error);
  if (code) {
    error->code = code;
    error->text = pn_strdup(text);
  }
  return code;
}

int pn_error_vformat(pn_error_t *error, int code, const char *fmt, va_list ap)
{
  char text[1024];
  int n = vsnprintf(text, 1024, fmt, ap);
  if (n >= 1024) {
    text[1023] = '\0';
  }
  return pn_error_set(error, code, text);
}

int pn_error_format(pn_error_t *error, int code, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int rcode = pn_error_vformat(error, code, fmt, ap);
  va_end(ap);
  return rcode;
}

int pn_error_from_errno(pn_error_t *error, const char *msg)
{
  char err[1024];
  strerror_r(errno, err, 1024);
  int code = PN_ERR;
  if (errno == EINTR)
      code = PN_INTR;
  return pn_error_format(error, code, "%s: %s", msg, err);
}

int pn_error_code(pn_error_t *error)
{
  return error ? error->code : PN_ARG_ERR;
}

const char *pn_error_text(pn_error_t *error)
{
  return error ? error->text : NULL;
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
  default: return "<unknown>";
  }
}
