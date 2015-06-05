#ifndef DECODER_H
#define DECODER_H
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

namespace proton {
namespace reactor {

class Value;

/**
Stream-like decoder from AMQP bytes to a stream of C++ values.

@see types.h defines C++ typedefs and types for AMQP each type. These types can
all be extracted from the corresponding AMQP type. In additon operator>> will do
the following conversions from AMQP to C++ types:

+-----------------------------------------+--------------------------------------------------+
|Target C++ type                          |Allowed AMQP types                                |
+=========================================+==================================================+
|bool                                     |Bool                                              |
|-----------------------------------------+--------------------------------------------------|
|signed integer type                      |Byte,Short,Int,Long [1]                           |
+-----------------------------------------+--------------------------------------------------+
|unsigned integer type                    |UByte,UShort,UInt,ULong [1]                       |
+-----------------------------------------+--------------------------------------------------+
|float or double                          |Float or Double                                   |
+-----------------------------------------+--------------------------------------------------+
|Value                                    |Any type                                          |
+-----------------------------------------+--------------------------------------------------+
|std::string                              |String, Binary, Symbol                            |
+-----------------------------------------+--------------------------------------------------+
|wchar_t                                  |Char                                              |
+-----------------------------------------+--------------------------------------------------+
|std::map<K, T>                           |Map with keys that convert to K and data that     |
|                                         |converts to T                                     |
+-----------------------------------------+--------------------------------------------------+
|Map                                      |Map may have mixed keys and data types            |
+-----------------------------------------+--------------------------------------------------+
|std::vector<T>                           |List or Array if data converts to T               |
+-----------------------------------------+--------------------------------------------------+
|List                                     |List, may have mixed types and datas              |
+-----------------------------------------+--------------------------------------------------+

You can disable conversions and force an exact type match using @see exact()
*/
class Decoder : public virtual Data {
  public:
    /** Raised if a Decoder operation fails  */
    struct Error : public ProtonException {
        explicit Error(const std::string& msg) throw() : ProtonException(msg) {}
    };

    PN_CPP_EXTERN Decoder();
    PN_CPP_EXTERN ~Decoder();

    /** Copy AMQP data from a byte buffer into the Decoder. */
    PN_CPP_EXTERN Decoder(const char* buffer, size_t size);

    /** Copy AMQP data from a std::string into the Decoder. */
    PN_CPP_EXTERN Decoder(const std::string&);

    /** Decode AMQP data from a byte buffer onto the end of the value stream. */
    PN_CPP_EXTERN void decode(const char* buffer, size_t size);

    /** Decode AMQP data from bytes in std::string onto the end of the value stream. */
    PN_CPP_EXTERN void decode(const std::string&);

    /** Return true if there are more values to read at the current level. */
    PN_CPP_EXTERN bool more() const;

    /** Type of the next value that will be read by operator>>
     *@throw Error if empty().
     */
    PN_CPP_EXTERN TypeId type() const;

    /** @defgroup decoder_simple_types Extract simple types, @see Decoder for details.
     *@throw Error if the Decoder is empty or the current value has an incompatible type.
     *@{
     */
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Bool&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Ubyte&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Byte&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Ushort&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Short&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Uint&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Int&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Char&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Ulong&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Long&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Timestamp&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Float&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Double&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Decimal32&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Decimal64&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Decimal128&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Uuid&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, std::string&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Value&);
    ///@}

    ///@internal
    template <class T> struct ExactRef { T& value; ExactRef(T& ref) : value(ref) {} };

    /** @see exact() */
  template <class T> friend Decoder& operator>>(Decoder&, ExactRef<T>);

  private:
    PN_CPP_EXTERN Decoder(pn_data_t*);
    template <class T> Decoder& extract(T& value);
    void checkType(TypeId);

    // Not implemented
    Decoder(const Decoder&);
    Decoder& operator=(const Decoder&);

  friend class Value;
  friend class Encoder;
};

/**
 * exact() disables the conversions allowed by Decoder operator>> and requires exact type match.
 *
 * For example the following will throw Decode::Error unless decoder conntains
 * an AMQP bool and an AMQP ULong.
 *
 * @code
 * Bool b;
 * ULong ul;
 * decoder >> exact(b) >> exact(ul)
 * @code
 */
template <class T> Decoder::ExactRef<T> exact(T& value) {
    return Decoder::ExactRef<T>(value);
}

///@see exact()
template <class T> Decoder& operator>>(Decoder& d, Decoder::ExactRef<T> ref) {
    d.checkType(TypeIdOf<T>::value);
    d >> ref.value;
}

}} // namespace proton::reactor
#endif // DECODER_H
