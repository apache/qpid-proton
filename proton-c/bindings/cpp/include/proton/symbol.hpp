#ifndef SYMBOL_HPP
#define SYMBOL_HPP
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

namespace proton {

/// symbol is a std::string that represents the AMQP symbol type.
/// A symbol can only contain 7-bit ASCII characters.
///
class symbol : public std::string {
  public:
    symbol(const std::string& s=std::string()) : std::string(s) {}
    symbol(const char* s) : std::string(s) {}
};

}

#endif // SYMBOL_HPP
