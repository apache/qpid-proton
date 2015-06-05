#ifndef ENCODER_H
#define ENCODER_H
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

#include "proton/cpp/Data.h"
#include "proton/cpp/types.h"
#include "proton/cpp/exceptions.h"
#include <iosfwd>

struct pn_data_t;

namespace proton {
namespace reactor {

class Value;

/**
Stream-like encoder from C++ values to AMQP bytes.

@see types.h defines C++ typedefs and types for AMQP each type. These types
insert as the corresponding AMQP type. Normal C++ conversion rules apply if you
insert any other type.

*/
class Encoder : public virtual Data {
  public:
    /** Raised if a Encoder operation fails  */
    struct Error : public ProtonException {
        explicit Error(const std::string& msg) throw() : ProtonException(msg) {}
    };

    PN_CPP_EXTERN Encoder();
    PN_CPP_EXTERN ~Encoder();

    /**
     * Encode the current values into buffer and update size to reflect the number of bytes encoded.
     *
     * Clears the encoder.
     *
     *@return if buffer==0 or size is too small then return false and  size to the required size.
     *Otherwise return true and set size to the number of bytes encoded.
     */
    PN_CPP_EXTERN bool encode(char* buffer, size_t& size);

    /** Encode the current values into a std::string, resize the string if necessary.
     *
     * Clears the encoder.
     */
    PN_CPP_EXTERN void encode(std::string&);

    /** Encode the current values into a std::string. Clears the encoder. */
    PN_CPP_EXTERN std::string encode();

    /** @defgroup encoder_simple_types Insert simple types.
     *@{
     */
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Bool);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Ubyte);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Byte);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Ushort);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Short);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Uint);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Int);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Char);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Ulong);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Long);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Timestamp);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Float);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Double);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Decimal32);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Decimal64);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Decimal128);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Uuid);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, String);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Symbol);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Binary);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, const Value&);
    ///@}

  private:
    PN_CPP_EXTERN Encoder(pn_data_t* pd); // Does not own.

    // Not implemented
    Encoder(const Encoder&);
    Encoder& operator=(const Encoder&);

  friend class Value;
};

}}
#endif // ENCODER_H
