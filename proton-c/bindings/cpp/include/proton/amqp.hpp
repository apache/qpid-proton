#ifndef PROTON_AMQP_HPP
#define PROTON_AMQP_HPP
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

#include <proton/types_fwd.hpp>

#include <string>

namespace proton {


/// This namespace contains typedefs to associate AMQP scalar type names with
/// the corresponding C++ types. These are provided as a convenience for those
/// familiar with AMQP, you do not need to use them, you can use the C++ types
/// directly.
///
/// The typedef names have a _type suffix to avoid ambiguity with C++ reserved
/// and std library type names.
///
namespace amqp {

///@name Typedefs for AMQP numeric types.
///@{
///@ Boolean true or false.
typedef bool boolean_type;
///@ 8-bit unsigned byte 
typedef uint8_t ubyte_type;
///@ 8-bit signed byte
typedef int8_t byte_type;
///@ 16-bit unsigned short integer
typedef uint16_t ushort_type;
///@ 16-bit signed short integer
typedef int16_t short_type;
///@ 32-bit unsigned integer
typedef uint32_t uint_type;
///@ 32-bit signed integer
typedef int32_t int_type;
///@ 64-bit unsigned long integer
typedef uint64_t ulong_type;
///@ 64-bit signed long integer
typedef int64_t long_type;
///@ 32-bit unicode code point
typedef wchar_t char_type;
///@ 32-bit binary floating point
typedef float float_type;
///@ 64-bit binary floating point
typedef double double_type;
///@}

/// An AMQP string is unicode  UTF-8 encoded.
typedef std::string string_type;

/// An AMQP string is ASCII 7-bit  encoded.
typedef proton::symbol symbol_type;

/// An AMQP binary contains variable length raw binary data.
typedef proton::binary binary_type;

/// A timestamp in milliseconds since the epoch 00:00:00 (UTC), 1 January 1970.
typedef proton::timestamp timestamp_type;

/// A 16-byte universally unique identifier.
typedef proton::uuid uuid_type;

///@name AMQP decimal floating point types.
///
/// These are not usable as arithmetic types in C++. You can pass them on over
/// AMQP or convert the raw bytes using a decimal support library. @see proton::decimal.
/// @{
typedef proton::decimal32 decimal32_type;
typedef proton::decimal64 decimal64_type;
typedef proton::decimal128 decimal128_type;
///@}

}}

#endif // PROTON_AMQP_HPP
