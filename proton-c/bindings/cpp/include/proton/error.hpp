#ifndef PROTON_CPP_EXCEPTIONS_H
#define PROTON_CPP_EXCEPTIONS_H

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

#include "proton/config.hpp"
#include "proton/export.hpp"

#include <stdexcept>
#include <string>

namespace proton {

/// The base proton error.
///
/// All exceptions thrown from functions in the proton namespace are
/// subclasses of proton::error.
struct
PN_CPP_CLASS_EXTERN error : public std::runtime_error {
    PN_CPP_EXTERN explicit error(const std::string&); ///< Construct with message
};

/// Raised if a timeout expires.
struct
PN_CPP_CLASS_EXTERN timeout_error : public error {
    PN_CPP_EXTERN explicit timeout_error(const std::string&);  ///< Construct with message
};

/// Raised if there is an error converting between AMQP and C++ data.
struct
PN_CPP_CLASS_EXTERN conversion_error : public error {
    PN_CPP_EXTERN explicit conversion_error(const std::string&);  ///< Construct with message
};

}

#endif // PROTON_CPP_EXCEPTIONS_H
