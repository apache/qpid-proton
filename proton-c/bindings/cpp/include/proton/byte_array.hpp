#ifndef BYTE_ARRAY_HPP
#define BYTE_ARRAY_HPP
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

#include "proton/export.hpp"
#include <proton/types_fwd.hpp>
#include <proton/comparable.hpp>

#include <algorithm>
#include <iterator>

namespace proton {
namespace internal {
PN_CPP_EXTERN void print_hex(std::ostream& o, const uint8_t* p, size_t n);
}

/// Used to represent fixed-sized data types that don't have a natural C++ representation
/// as an array of bytes.
template <size_t N> class byte_array : private comparable<byte_array<N> > {
  public:
    ///@name Sequence container typedefs
    ///@{
    typedef uint8_t                                   value_type;
    typedef value_type*			              pointer;
    typedef const value_type*                         const_pointer;
    typedef value_type&                   	      reference;
    typedef const value_type&             	      const_reference;
    typedef value_type*          		      iterator;
    typedef const value_type*			      const_iterator;
    typedef std::size_t                    	      size_type;
    typedef std::ptrdiff_t                   	      difference_type;
    typedef std::reverse_iterator<iterator>	      reverse_iterator;
    typedef std::reverse_iterator<const_iterator>     const_reverse_iterator;
    ///@}

    /// 0-initialized byte array
    byte_array() { std::fill(bytes_, bytes_+N, '\0'); }

    /// Size of the array
    static size_t size() { return N; }

    ///@name Array operators
    ///@{
    value_type* begin() { return bytes_; }
    value_type* end() { return bytes_+N; }
    value_type& operator[](size_t i) { return bytes_[i]; }

    const value_type* begin() const { return bytes_; }
    const value_type* end() const { return bytes_+N; }
    const value_type& operator[](size_t i) const { return bytes_[i]; }
    ///@}

    ///@name Comparison operators
    ///@{
  friend bool operator==(const byte_array& x, const byte_array& y) {
      return std::equal(x.begin(), x.end(), y.begin());
  }

  friend bool operator<(const byte_array& x, const byte_array& y) {
      return std::lexicographical_compare(x.begin(), x.end(), y.begin(), y.end());
  }
    ///@}

    /// Print byte array in hex.
  friend std::ostream& operator<<(std::ostream& o, const byte_array& b) {
      internal::print_hex(o, b.begin(), b.size());
      return o;
  }

  private:
    value_type bytes_[N];
};

}

#endif // BYTE_ARRAY_HPP
