#ifndef PROTON_ERROR_HPP
#define PROTON_ERROR_HPP

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

#include "./internal/config.hpp"
#include "./internal/export.hpp"

#include <stdexcept>
#include <string>

/// @file
/// @copybrief proton::error

namespace proton {

/// The base Proton error.
///
/// All exceptions thrown from functions in the proton namespace are
/// subclasses of proton::error.
struct
PN_CPP_CLASS_EXTERN error : public std::runtime_error {
    /// Construct the error with a message.
    PN_CPP_EXTERN explicit error(const std::string&);
};

/// An operation timed out.
struct
PN_CPP_CLASS_EXTERN timeout_error : public error {
    /// Construct the error with a message.
    PN_CPP_EXTERN explicit timeout_error(const std::string&);
};

/// An error converting between AMQP and C++ data.
struct
PN_CPP_CLASS_EXTERN conversion_error : public error {
    /// Construct the error with a message.
    PN_CPP_EXTERN explicit conversion_error(const std::string&);
};

} // proton

#endif // PROTON_ERROR_HPP
