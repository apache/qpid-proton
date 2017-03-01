#ifndef CORE_URL_INTERNAL_H
#define CORE_URL_INTERNAL_H
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

/**@file Simple URL parser for internal use only */

/** Parse a URL in-place. The field pointers scheme, user and so on are made to point to the
 * decoded fields, which are stored in the same memory as the original URL.
 * You must not try to use url as the URL string, but you are still responsible for freeing it.
 */
PN_EXTERN void pni_parse_url(char *url, char **scheme, char **user, char **pass, char **host, char **port, char **path);

#endif  /*!CORE_URL_INTERNAL_H*/
