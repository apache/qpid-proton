#ifndef PROTON_URL_H
#define PROTON_URL_H
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

#include <proton/import_export.h>

#ifdef __cplusplus
extern "C" {
#endif


/** @file
 * URL API for parsing URLs.
 *
 * @defgroup url URL
 * @{
 */

/** A parsed URL */
typedef struct pn_url_t pn_url_t;

/** Create an empty URL */
PNX_EXTERN pn_url_t *pn_url(void);

/** Parse a string URL as a pn_url_t.
 *
 * URL syntax:
 *
 *     [ <scheme> :// ] [ <user> [ : <password> ] @ ] <host> [ : <port> ] [ / <path> ]
 *
 * `scheme`, `user`, `password`, `port` cannot contain any of '@', ':', '/'
 *
 * If the first character of `host` is '[' then it can contain any character up
 * to ']' (this is to allow IPv6 literal syntax). Otherwise it also cannot
 * contain '@', ':', '/'
 *
 * `path` can contain any character
 *
 *@param[in] url A URL string.
 *@return The parsed pn_url_t or NULL if url is not a valid URL string.
 */
PNX_EXTERN pn_url_t *pn_url_parse(const char *url);

/** Free a URL */
PNX_EXTERN void pn_url_free(pn_url_t *url);

/** Clear the contents of the URL. */
PNX_EXTERN void pn_url_clear(pn_url_t *url);

/**
 * Return the string form of a URL.
 *
 *  The returned string is owned by the pn_url_t and will become invalid if it
 *  is modified.
 */
PNX_EXTERN const char *pn_url_str(pn_url_t *url);

/**
 *@name Getters for parts of the URL.
 *
 *Values belong to the URL. May return NULL if the value is not set.
 *
 *@{
 */
PNX_EXTERN const char *pn_url_get_scheme(pn_url_t *url);
PNX_EXTERN const char *pn_url_get_username(pn_url_t *url);
PNX_EXTERN const char *pn_url_get_password(pn_url_t *url);
PNX_EXTERN const char *pn_url_get_host(pn_url_t *url);
PNX_EXTERN const char *pn_url_get_port(pn_url_t *url);
PNX_EXTERN const char *pn_url_get_path(pn_url_t *url);
///@}

/**
 *@name Setters for parts of the URL.
 *
 *Values are copied. Value can be NULL to indicate the part is not set.
 *
 *@{
 */
PNX_EXTERN void pn_url_set_scheme(pn_url_t *url, const char *scheme);
PNX_EXTERN void pn_url_set_username(pn_url_t *url, const char *username);
PNX_EXTERN void pn_url_set_password(pn_url_t *url, const char *password);
PNX_EXTERN void pn_url_set_host(pn_url_t *url, const char *host);
PNX_EXTERN void pn_url_set_port(pn_url_t *url, const char *port);
PNX_EXTERN void pn_url_set_path(pn_url_t *url, const char *path);
///@}

///@}

#ifdef __cplusplus
}
#endif

#endif
