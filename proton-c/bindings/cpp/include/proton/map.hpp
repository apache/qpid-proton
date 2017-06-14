#ifndef PROTON_CPP_MAP_H
#define PROTON_CPP_MAP_H

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

#include "./value.hpp"

#include "internal/pn_unique_ptr.hpp"

#include <cstddef>

namespace proton {

namespace codec {
class decoder;
class encoder;
}

template <class K, class T>
class map_type_impl;

template <class K, class T>
class map;

/// Decode from a proton::map
template <class K, class T>
PN_CPP_EXTERN proton::codec::decoder& operator>>(proton::codec::decoder& d, map<K,T>& m);
/// Encode to a proton::map
template <class K, class T>
PN_CPP_EXTERN proton::codec::encoder& operator<<(proton::codec::encoder& e, const map<K,T>& m);
/// Swap proton::map instances
template <class K, class T>
PN_CPP_EXTERN void swap(map<K,T>&, map<K,T>&);

/// Used to access standard AMQP property, annotation and filter maps attached
/// to proton::message and proton::source.
///
/// Provides only basic get()/set() operations for convenience.  For more
/// complicated use (iteration, preserving order etc.) you should convert to a
/// standard C++ map type such as std::map. See @ref message_properties.cpp
/// and @ref types_page.
///
template <class K, class T>
class PN_CPP_CLASS_EXTERN map {
    template <class M, class U=void>
        struct assignable_map :
            public internal::enable_if<codec::is_encodable_map<M,K,T>::value, U> {};

 public:
    /// @name Construct and assign
    /// @{
    PN_CPP_EXTERN map();
    PN_CPP_EXTERN map(const map& cm);
    PN_CPP_EXTERN map& operator=(const map& cm);
#if PN_CPP_HAS_RVALUE_REFERENCES
    PN_CPP_EXTERN map(map&&);
    PN_CPP_EXTERN map& operator=(map&&);
#endif
    ///@}
    PN_CPP_EXTERN ~map();

    /// Type-safe assign from a compatible map, e.g. std::map<K,T> - see @ref types_page
    template <class M>
    typename assignable_map<M, map&>::type operator=(const M& x) { value(x); return *this;}

    /// Copy from a proton::value.
    /// @throw proton::conversion_error if x does not contain a compatible map.
    PN_CPP_EXTERN void value(const value& x);
    /// Access as a proton::value containing an AMQP map
    PN_CPP_EXTERN proton::value& value();
    /// Access as a proton::value containing an AMQP map
    PN_CPP_EXTERN const proton::value& value() const;

    /// Get the map entry for key k, return T() if no such entry
    PN_CPP_EXTERN T get(const K& k) const;
    /// Put a map entry for key k.
    PN_CPP_EXTERN void put(const K& k, const T& v);
    /// Erase the map entry at k
    PN_CPP_EXTERN size_t erase(const K& k);
    /// True if there is a map entry for k
    PN_CPP_EXTERN bool exists(const K& k) const;
    /// Number of map entries
    PN_CPP_EXTERN size_t size() const;
    /// Clear the map
    PN_CPP_EXTERN void clear();
    /// True if the map is empty
    PN_CPP_EXTERN bool empty() const;

    ///@cond INTERNAL
    explicit map(pn_data_t*);
    void reset(pn_data_t*);

 private:
    typedef map_type_impl<K,T> map_type;
    mutable internal::pn_unique_ptr<map_type> map_;
    mutable proton::value value_;

    map_type& cache() const;
    proton::value& flush() const;

  friend PN_CPP_EXTERN proton::codec::decoder& operator>> <>(proton::codec::decoder&, map&);
  friend PN_CPP_EXTERN proton::codec::encoder& operator<< <>(proton::codec::encoder&, const map&);
  friend PN_CPP_EXTERN void swap<>(map&, map&);
  /// @endcond
};

} // namespace proton

#endif // PROTON_CPP_MAP_H
