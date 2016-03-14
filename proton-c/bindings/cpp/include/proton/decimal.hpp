#ifndef DECIMAL_HPP
#define DECIMAL_HPP
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

#include "proton/byte_array.hpp"
#include "proton/comparable.hpp"
#include "proton/export.hpp"

#include <proton/types.h>

#include <iosfwd>

namespace proton {

///@name AMQP decimal types.
///
/// AMQP uses the standard IEEE 754-2008 encoding for decimal types.
///
/// This library does not provide support for decimal arithmetic but it does
/// provide access to the byte representation of decimal values. You can pass
/// these values uninterpreted via AMQP, or you can use a library that supports
/// IEEE 754-2008 and make a byte-wise copy between the real decimal values and
/// proton::decimal values.
///
/// @{

/// 32-bit decimal floating point.
class decimal32 : public byte_array<4> {};

/// 64-bit decimal floating point.
class decimal64 : public byte_array<8> {};

/// 128-bit decimal floating point.
class decimal128 : public byte_array<16> {};
///@}

/// Print decimal values
///@{
PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const decimal32&);
PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const decimal64&);
PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const decimal128&);
///@}


}

#endif // DECIMAL_HPP
