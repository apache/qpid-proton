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

#include "core/url-internal.h"
#include <string.h>
#include <stdlib.h>

static void pni_urldecode(const char *src, char *dst)
{
  const char *in = src;
  char *out = dst;
  while (*in != '\0')
  {
    if ('%' == *in)
    {
      if ((in[1] != '\0') && (in[2] != '\0'))
      {
        char esc[3];
        esc[0] = in[1];
        esc[1] = in[2];
        esc[2] = '\0';
        unsigned long d = strtoul(esc, NULL, 16);
        *out = (char)d;
        in += 3;
        out++;
      }
      else
      {
        *out = *in;
        in++;
        out++;
      }
    }
    else
    {
      *out = *in;
      in++;
      out++;
    }
  }
  *out = '\0';
}

void pni_parse_url(char *url, char **scheme, char **user, char **pass, char **host, char **port, char **path)
{
  if (!url) return;
  *scheme = *user = *pass = *host = *port = *path = NULL;

  char *slash = strchr(url, '/');

  if (slash && slash>url) {
    char *scheme_end = strstr(slash-1, "://");

    if (scheme_end && scheme_end<slash) {
      *scheme_end = '\0';
      *scheme = url;
      url = scheme_end + 3;
      slash = strchr(url, '/');
    }
  }

  if (slash) {
    *slash = '\0';
    *path = slash + 1;
  }

  char *at = strchr(url, '@');
  if (at) {
    *at = '\0';
    char *up = url;
    *user = up;
    url = at + 1;
    char *colon = strchr(up, ':');
    if (colon) {
      *colon = '\0';
      *pass = colon + 1;
    }
  }

  *host = url;
  char *open = (*url == '[') ? url : 0;
  if (open) {
    char *close = strchr(open, ']');
    if (close) {
        *host = open + 1;
        *close = '\0';
        url = close + 1;
    }
  }

  char *colon = strrchr(url, ':'); /* Be flexible about unbracketed IPV6 literals like ::1:<port> */
  if (colon) {
    *colon = '\0';
    *port = colon + 1;
  }

  if (*user) pni_urldecode(*user, *user);
  if (*pass) pni_urldecode(*pass, *pass);
}
