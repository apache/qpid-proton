#ifndef _PROTON_VALUE_INTERNAL_H
#define _PROTON_VALUE_INTERNAL_H 1

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

#include <proton/value.h>

struct pn_symbol_t {
  size_t size;
  char name[];
};

struct pn_string_t {
  size_t size;
  wchar_t wcs[];
};

struct pn_binary_t {
  size_t size;
  char bytes[];
};

struct pn_array_t {
  enum TYPE type;
  size_t size;
  size_t capacity;
  pn_value_t values[];
};

struct pn_list_t {
  size_t size;
  size_t capacity;
  pn_value_t values[];
};

struct pn_map_t {
  size_t size;
  size_t capacity;
  pn_value_t pairs[];
};

#endif /* value-internal.h */
