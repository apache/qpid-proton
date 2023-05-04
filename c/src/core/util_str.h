#ifndef _PROTON_SRC_UTIL_STR_H
#define _PROTON_SRC_UTIL_STR_H 1

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

#include <stddef.h>

#if __cplusplus
extern "C" {
#endif

char *pn_strdup(const char *src);
char *pn_strndup(const char *src, size_t n);

int pn_strcasecmp(const char* a, const char* b);
int pn_strncasecmp(const char* a, const char* b, size_t len);


#if __cplusplus
}
#endif

#endif /* util_str.h */
