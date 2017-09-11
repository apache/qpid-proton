#ifndef PROTON_DECIMAL_HPP
#define PROTON_DECIMAL_HPP

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

#include "./byte_array.hpp"
#include "./internal/export.hpp"
#include "./internal/comparable.hpp"

#include <iosfwd>

/// @file
///
/// AMQP decimal types.
///
/// AMQP uses the standard IEEE 754-2008 encoding for decimal types.
///
/// This library does not provide support for decimal arithmetic, but
/// it does provide access to the byte representation of decimal
/// values. You can pass these values uninterpreted via AMQP, or you
/// can use a library that supports IEEE 754-2008 and make a byte-wise
/// copy between the real decimal values and `proton::decimal` values.

namespace proton {

/// A 32-bit decimal floating-point value.
class decimal32 : public byte_array<4> {};

/// A 64-bit decimal floating-point value.
class decimal64 : public byte_array<8> {};

/// A 128-bit decimal floating-point value.
class decimal128 : public byte_array<16> {};

/// Print a 32-bit decimal value.    
PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const decimal32&);

/// Print a 64-bit decimal value.    
PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const decimal64&);

/// Print a 128-bit decimal value.    
PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const decimal128&);

} // proton

#endif // PROTON_DECIMAL_HPP
