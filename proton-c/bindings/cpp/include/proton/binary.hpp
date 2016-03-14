#ifndef BINARY_HPP
#define BINARY_HPP
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

#include <proton/export.hpp>
#include <proton/types_fwd.hpp>

#include <iosfwd>
#include <vector>

namespace proton {

/// Arbitrary binary data.
class binary : public std::vector<uint8_t> {
  public:
    ///@name Constructors @{
    explicit binary() : std::vector<value_type>() {}
    explicit binary(size_t n) : std::vector<value_type>(n) {}
    explicit binary(size_t n, value_type x) : std::vector<value_type>(n, x) {}
    explicit binary(const std::string& s) : std::vector<value_type>(s.begin(), s.end()) {}
    template <class Iter> binary(Iter first, Iter last) : std::vector<value_type>(first, last) {}
    ///@}

    /// Convert to std::string
    operator std::string() const { return std::string(begin(), end()); }

    /// Assignment
    binary& operator=(const std::string& x) { assign(x.begin(), x.end()); return *this; }
};

/// Print binary value
PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const binary&);

}

#endif // BINARY_HPP
