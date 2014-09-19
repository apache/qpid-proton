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

#include <proton/url.h>
#include <proton/util.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static char* copy(const char* str) {
    if (str ==  NULL) return NULL;
    char *str2 = (char*)malloc(strlen(str));
    if (str2) strcpy(str2, str);
    return str2;
}

struct pn_url_t {
    char *scheme;
    char *username;
    char *password;
    char *host;
    char *port;
    char *path;
    char *str;
};

PN_EXTERN pn_url_t *pn_url() {
    pn_url_t *url = (pn_url_t*)malloc(sizeof(pn_url_t));
    memset(url, 0, sizeof(*url));
    return url;
}

/** Parse a string URL as a pn_url_t.
 *@param[in] url A URL string.
 *@return The parsed pn_url_t or NULL if url is not a valid URL string.
 */
PN_EXTERN pn_url_t *pn_url_parse(const char *str) {
    if (!str || !*str)          /* Empty string or NULL is illegal. */
        return NULL;

    pn_url_t *url = pn_url();
    char *str2 = copy(str);         /* FIXME aconway 2014-09-19: clean up */
    pni_parse_url(str2, &url->scheme, &url->username, &url->password, &url->host, &url->port, &url->path);
    url->scheme = copy(url->scheme);
    url->username = copy(url->username);
    url->password = copy(url->password);
    url->host = (url->host && !*url->host) ? NULL : copy(url->host);
    url->port = copy(url->port);
    url->path = copy(url->path);
    return url;
}

/** Free a URL */
PN_EXTERN void pn_url_free(pn_url_t *url) {
    pn_url_clear(url);
    free(url);
}

/** Clear the contents of the URL. */
PN_EXTERN void pn_url_clear(pn_url_t *url) {
    pn_url_set_username(url, NULL);
    pn_url_set_password(url, NULL);
    pn_url_set_host(url, NULL);
    pn_url_set_port(url, NULL);
    pn_url_set_path(url, NULL);
    free(url->str); url->str = NULL;
}

static inline int len(const char *str) { return str ? strlen(str) : 0; }

/** Return the string form of a URL. */
PN_EXTERN const char *pn_url_str(pn_url_t *url) {
    int size = len(url->scheme) + len(url->username) + len(url->password)
        + len(url->host) + len(url->port) + len(url->path)
        + len("s://u:p@[h]:p/p");
    free(url->str);
    url->str = (char*)malloc(size);
    if (!url->str) return NULL;

    int i = 0;
    if (url->scheme) i += snprintf(url->str+i, size-i, "%s://", url->scheme);
    if (url->username) i += snprintf(url->str+i, size-i, "%s", url->username);
    if (url->password) i += snprintf(url->str+i, size-i, ":%s", url->password);
    if (url->username || url->password) i += snprintf(url->str+i, size-i, "@");
    if (url->host) {
        if (strchr(url->host, ':')) i += snprintf(url->str+i, size-i, "[%s]", url->host);
        else i += snprintf(url->str+i, size-i, "%s", url->host);
    }
    if (url->port) i += snprintf(url->str+i, size-i, ":%s", url->port);
    if (url->path) i += snprintf(url->str+i, size-i, "/%s", url->path);
    return url->str;
}

PN_EXTERN const char *pn_url_scheme(pn_url_t *url) { return url->scheme; }
PN_EXTERN const char *pn_url_username(pn_url_t *url) { return url->username; }
PN_EXTERN const char *pn_url_password(pn_url_t *url) { return url->password; }
PN_EXTERN const char *pn_url_host(pn_url_t *url) { return url->host; }
PN_EXTERN const char *pn_url_port(pn_url_t *url) { return url->port; }
PN_EXTERN const char *pn_url_path(pn_url_t *url) { return url->path; }

#define SET(part) free(url->part); url->part = copy(part)
PN_EXTERN void pn_url_set_scheme(pn_url_t *url, const char *scheme) { SET(scheme); }
PN_EXTERN void pn_url_set_username(pn_url_t *url, const char *username) { SET(username); }
PN_EXTERN void pn_url_set_password(pn_url_t *url, const char *password) { SET(password); }
PN_EXTERN void pn_url_set_host(pn_url_t *url, const char *host) { SET(host); }
PN_EXTERN void pn_url_set_port(pn_url_t *url, const char *port) { SET(port); }
PN_EXTERN void pn_url_set_path(pn_url_t *url, const char *path) { SET(path); }


