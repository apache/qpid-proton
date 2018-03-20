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

#include "core/util.h"
#include "proton/url.h"
#include "proton/object.h"

#include <stdlib.h>
#include <string.h>

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

/* Not static - used by messenger.c */
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

/** URL-encode src and append to dst. */
static void pni_urlencode(pn_string_t *dst, const char* src) {
    static const char *bad = "@:/";

    if (!src) return;
    const char *i = src;
    const char *j = strpbrk(i, bad);
    while (j) {
        pn_string_addf(dst, "%.*s", (int)(j-i), i);
        pn_string_addf(dst, "%%%02X", (int)*j);
        i = j + 1;
        j = strpbrk(i, bad);
    }
    pn_string_addf(dst, "%s", i);
}

struct pn_url_t {
    char *scheme;
    char *username;
    char *password;
    char *host;
    char *port;
    char *path;
    pn_string_t *str;
};

/** Internal use only, returns the pn_string_t. Public function is pn_url_str() */
static pn_string_t *pn_url_string(pn_url_t* url)
{
    pn_url_str(url);               /* Make sure str is up to date */
    return url->str;
}

static void pn_url_finalize(void *object)
{
    pn_url_t *url = (pn_url_t *) object;
    pn_url_clear(url);
    pn_free(url->str);
}

static uintptr_t pn_url_hashcode(void *object)
{
    pn_url_t *url = (pn_url_t *) object;
    return pn_hashcode(pn_url_string(url));
}

static intptr_t pn_url_compare(void *oa, void *ob)
{
    pn_url_t *a = (pn_url_t *) oa;
    pn_url_t *b = (pn_url_t *) ob;
    return pn_compare(pn_url_string(a), pn_url_string(b));
}


static int pn_url_inspect(void *obj, pn_string_t *dst)
{
    pn_url_t *url = (pn_url_t *) obj;
    int err = 0;
    err = pn_string_addf(dst, "Url("); if (err) return err;
    err = pn_inspect(pn_url_string(url), dst); if (err) return err;
    return pn_string_addf(dst, ")");
}

#define pn_url_initialize NULL


pn_url_t *pn_url() {
    static const pn_class_t clazz = PN_CLASS(pn_url);
    pn_url_t *url = (pn_url_t*) pn_class_new(&clazz, sizeof(pn_url_t));
    if (!url) return NULL;
    memset(url, 0, sizeof(*url));
    url->str = pn_string(NULL);
    return url;
}

/** Parse a string URL as a pn_url_t.
 *@param[in] url A URL string.
 *@return The parsed pn_url_t or NULL if url is not a valid URL string.
 */
pn_url_t *pn_url_parse(const char *str) {
    if (!str || !*str)          /* Empty string or NULL is illegal. */
        return NULL;

    pn_url_t *url = pn_url();
    char *str2 = pn_strdup(str);
    pni_parse_url(str2, &url->scheme, &url->username, &url->password, &url->host, &url->port, &url->path);
    url->scheme = pn_strdup(url->scheme);
    url->username = pn_strdup(url->username);
    url->password = pn_strdup(url->password);
    url->host = (url->host && !*url->host) ? NULL : pn_strdup(url->host);
    url->port = pn_strdup(url->port);
    url->path = pn_strdup(url->path);

    free(str2);
    return url;
}

/** Free a URL */
void pn_url_free(pn_url_t *url) { pn_free(url); }

/** Clear the contents of the URL. */
void pn_url_clear(pn_url_t *url) {
    pn_url_set_scheme(url, NULL);
    pn_url_set_username(url, NULL);
    pn_url_set_password(url, NULL);
    pn_url_set_host(url, NULL);
    pn_url_set_port(url, NULL);
    pn_url_set_path(url, NULL);
    pn_string_clear(url->str);
}

/** Return the string form of a URL. */
const char *pn_url_str(pn_url_t *url) {
    if (pn_string_get(url->str) == NULL) {
        pn_string_set(url->str, "");
        if (url->scheme) pn_string_addf(url->str, "%s://", url->scheme);
        if (url->username) pni_urlencode(url->str, url->username);
        if (url->password) {
            pn_string_addf(url->str, ":");
            pni_urlencode(url->str, url->password);
        }
        if (url->username || url->password) pn_string_addf(url->str, "@");
        if (url->host) {
            if (strchr(url->host, ':')) pn_string_addf(url->str, "[%s]", url->host);
            else pn_string_addf(url->str, "%s", url->host);
        }
        if (url->port) pn_string_addf(url->str, ":%s", url->port);
        if (url->path) pn_string_addf(url->str, "/%s", url->path);
    }
    return pn_string_get(url->str);
}

const char *pn_url_get_scheme(pn_url_t *url) { return url->scheme; }
const char *pn_url_get_username(pn_url_t *url) { return url->username; }
const char *pn_url_get_password(pn_url_t *url) { return url->password; }
const char *pn_url_get_host(pn_url_t *url) { return url->host; }
const char *pn_url_get_port(pn_url_t *url) { return url->port; }
const char *pn_url_get_path(pn_url_t *url) { return url->path; }

#define SET(part) free(url->part); url->part = pn_strdup(part); pn_string_clear(url->str)
void pn_url_set_scheme(pn_url_t *url, const char *scheme) { SET(scheme); }
void pn_url_set_username(pn_url_t *url, const char *username) { SET(username); }
void pn_url_set_password(pn_url_t *url, const char *password) { SET(password); }
void pn_url_set_host(pn_url_t *url, const char *host) { SET(host); }
void pn_url_set_port(pn_url_t *url, const char *port) { SET(port); }
void pn_url_set_path(pn_url_t *url, const char *path) { SET(path); }


