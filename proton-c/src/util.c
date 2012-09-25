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

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <proton/error.h>
#include <proton/util.h>

ssize_t pn_quote_data(char *dst, size_t capacity, const char *src, size_t size)
{
  int idx = 0;
  for (int i = 0; i < size; i++)
  {
    uint8_t c = src[i];
    if (isprint(c)) {
      if (idx < (int) (capacity - 1)) {
        dst[idx++] = c;
      } else {
        return PN_OVERFLOW;
      }
    } else {
      if (idx < (int) (capacity - 4)) {
        idx += sprintf(dst + idx, "\\x%.2x", c);
      } else {
        return PN_OVERFLOW;
      }
    }
  }

  dst[idx] = '\0';
  return idx;
}

void pn_fprint_data(FILE *stream, const char *bytes, size_t size)
{
  size_t capacity = 4*size + 1;
  char buf[capacity];
  ssize_t n = pn_quote_data(buf, capacity, bytes, size);
  if (n >= 0) {
    fputs(buf, stream);
  } else {
    fprintf(stderr, "pn_quote_data: %zi\n", n);
  }
}

void pn_print_data(const char *bytes, size_t size)
{
  pn_fprint_data(stdout, bytes, size);
}

void parse_url(char *url, char **scheme, char **user, char **pass, char **host, char **port, char **path)
{
  if (url) {
    char *scheme_end = strstr(url, "://");
    if (scheme_end) {
      *scheme_end = '\0';
      *scheme = url;
      url = scheme_end + 3;
    }

    char *at = index(url, '@');
    if (at) {
      *at = '\0';
      char *up = url;
      *user = up;
      url = at + 1;
      char *colon = index(up, ':');
      if (colon) {
        *colon = '\0';
        *pass = colon + 1;
      }
    }

    char *slash = index(url, '/');
    if (slash) {
      *slash = '\0';
      *host = url;
      url = slash + 1;
      *path = url;
    } else {
      *host = url;
    }

    char *colon = index(*host, ':');
    if (colon) {
      *colon = '\0';
      *port = colon + 1;
    }
  }
}

void pn_vfatal(char *fmt, va_list ap)
{
  vfprintf(stderr, fmt, ap);
  exit(EXIT_FAILURE);
}

void pn_fatal(char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  pn_vfatal(fmt, ap);
  va_end(ap);
}

bool pn_env_bool(const char *name)
{
  char *v = getenv(name);
  return v && (!strcasecmp(v, "true") || !strcasecmp(v, "1") ||
               !strcasecmp(v, "yes"));
}

char *pn_strdup(const char *src)
{
  if (src) {
    char *dest = malloc((strlen(src)+1)*sizeof(char));
    if (!dest) return NULL;
    return strcpy(dest, src);
  } else {
    return NULL;
  }
}

char *pn_strndup(const char *src, size_t n)
{
  if (src) {
    int size = 0;
    for (const char *c = src; size < n && *c; c++) {
      size++;
    }

    char *dest = malloc(size + 1);
    if (!dest) return NULL;
    strncpy(dest, src, n);
    dest[size] = '\0';
    return dest;
  } else {
    return NULL;
  }
}
