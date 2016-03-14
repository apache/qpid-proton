#ifndef PROTON_TYPES_FWD_HPP
#define PROTON_TYPES_FWD_HPP
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


///@file
///
/// Forward declarations for all the C++ types used by proton to represent AMQP types.

#include <proton/config.hpp>

#include <proton/type_compat.h>

#include <string>

/// The proton namespace
namespace proton {

class binary;
class decimal128;
class decimal32;
class decimal64;
class scalar;
class symbol;
class timestamp;
class duration;
class uuid;
class uuid;
class value;

struct null {
    null() {}
#if PN_CPP_HAS_CPP11
    null(std::nullptr_t) {}
#endif
};
}

#endif // PROTON_TYPES_FWD_HPP
