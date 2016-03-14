#ifndef PROTON_TYPES_HPP
#define PROTON_TYPES_HPP
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
/// Include the definitions of all proton types used to represent AMQP types.

/**@page types AMQP and C++ types

@details

An AMQP message body can hold binary data using any encoding you like. AMQP also
defines its own encoding and types. The AMQP encoding is often used in message
bodies because it is supported by AMQP libraries on many languages and
platforms. You also need to use the AMQP types to set and examine message
properties.

## Scalar types

Each type is identified by a proton::type_id.

C++ type            | AMQP type_id         | Description
--------------------|----------------------|-----------------------
bool                | proton::BOOLEAN      | Boolean true/false
uint8_t             | proton::UBYTE        | 8 bit unsigned byte
int8_t              | proton::BYTE         | 8 bit signed byte
uint16_t            | proton::USHORT       | 16 bit unsigned integer
int16_t             | proton::SHORT        | 16 bit signed integer
uint32_t            | proton::UINT         | 32 bit unsigned integer
int32_t             | proton::INT          | 32 bit signed integer
uint64_t            | proton::ULONG        | 64 bit unsigned integer
int64_t             | proton::LONG         | 64 bit signed integer
wchar_t             | proton::CHAR         | 32 bit unicode code point
float               | proton::FLOAT        | 32 bit binary floating point
double              | proton::DOUBLE       | 64 bit binary floating point
proton::timestamp   | proton::TIMESTAMP    | 64 bit signed milliseconds since 00:00:00 (UTC), 1 January 1970.
proton::decimal32   | proton::DECIMAL32    | 32 bit decimal floating point
proton::decimal64   | proton::DECIMAL64    | 64 bit decimal floating point
proton::decimal128  | proton::DECIMAL128   | 128 bit decimal floating point
proton::uuid        | proton::UUID         | 128 bit universally-unique identifier
std::string         | proton::STRING       | UTF-8 encoded unicode string
proton::symbol      | proton::SYMBOL       | 7-bit ASCII encoded string
proton::binary      | proton::BINARY       | Variable-length binary data

proton::scalar is a holder that can hold a scalar value of any type.

## Compound types

C++ type            | AMQP type_id         | Description
--------------------|----------------------|-----------------------
see below           | proton::ARRAY        | Sequence of values of the same type
see below           | proton::LIST         | Sequence of values of mixed types
see below           | proton::MAP          | Map of key/value pairs

proton::value is a holder that can hold any AMQP value, scalar or compound

proton::ARRAY converts to/from C++ sequences: std::vector, std::deque, std::list and
std::forward_list.

proton::LIST converts to/from sequences of proton::value or proton::scalar,
which can hold mixed types of data.

proton::MAP converts to/from std::map, std::unordered_map and sequences of
std::pair. 

When decoding the encoded map types must be convertible to element type of the
C++ sequence or the key/value types of the C++ map. Use proton::value as the
element or key/value type to decode any ARRAY/LIST/MAP.

For example you can decode any AMQP MAP into:

    std::map<proton::value, proton::value>

You can decode any AMQP LIST or ARRAY into

    std::vector<proton::value>

## Include files

You can simply include proton/types.hpp to include all the type definitions and
conversions. Alternatively, you can selectively include only what you need:

 - proton/types_fwd.hpp: forward declarations for all types.
 - proton/list.hpp, proton/vector.hpp etc.: conversions for std::list, std::vector etc.
 - include individual .hpp files as per the table above.
*/

// TODO aconway 2016-03-15: described types, described arrays.

#include <proton/annotation_key.hpp>
#include <proton/binary.hpp>
#include <proton/config.hpp>
#include <proton/decimal.hpp>
#include <proton/deque.hpp>
#include <proton/duration.hpp>
#include <proton/list.hpp>
#include <proton/map.hpp>
#include <proton/message_id.hpp>
#include <proton/scalar.hpp>
#include <proton/symbol.hpp>
#include <proton/timestamp.hpp>
#include <proton/types_fwd.hpp>
#include <proton/uuid.hpp>
#include <proton/value.hpp>
#include <proton/vector.hpp>

#include <proton/config.hpp>
#if PN_CPP_HAS_CPP11
#include <proton/forward_list.hpp>
#include <proton/unordered_map.hpp>
#endif

#endif // PROTON_TYPES_HPP
