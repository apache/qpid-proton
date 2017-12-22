#ifndef PROTON_SYMBOL_HPP
#define PROTON_SYMBOL_HPP

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

#include <string>

/// @file
/// @copybrief proton::symbol

namespace proton {

/// A string that represents the AMQP symbol type.
///
/// A symbol can contain only 7-bit ASCII characters.
class symbol : public std::string {
  public:
    /// Construct from a `std::string`.
    symbol(const std::string& s=std::string()) : std::string(s) {}

    /// Construct from a C string.
    symbol(const char* s) : std::string(s) {}

    /// Construct from any sequence of `char`.
    template<class Iter> symbol(Iter start, Iter finish) : std::string(start, finish) {}
};

} // proton

#endif // PROTON_SYMBOL_HPP
