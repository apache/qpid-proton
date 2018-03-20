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

#include "proton/internal/object.hpp"
#include <proton/object.h>

namespace proton {
namespace internal {

void pn_ptr_base::incref(void *p) {
    if (p) ::pn_incref(const_cast<void*>(p));
}

void pn_ptr_base::decref(void *p) {
    if (p) ::pn_decref(const_cast<void*>(p));
}

std::string pn_ptr_base::inspect(void* p) {
    if (!p) return std::string();
    ::pn_string_t* s = ::pn_string(NULL);
    (void) ::pn_inspect(p, s);
    return std::string(pn_string_get(s));
}
}}
