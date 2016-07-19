#ifndef PROTON_CPP_CACHED_MAP_H
#define PROTON_CPP_CACHED_MAP_H

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

#include "./config.hpp"
#include "./export.hpp"
#include "./pn_unique_ptr.hpp"

#include <cstddef>

namespace proton {

namespace codec {
class decoder;
class encoder;
}

namespace internal {

template <class key_type, class value_type>
class map_type_impl;

/// A convenience class to view and manage AMQP map data contained in
/// a proton::value.  An internal cache of the map data is created as
/// needed.
template <class K, class V>
class cached_map;

template <class K, class V>
PN_CPP_EXTERN proton::codec::decoder& operator>>(proton::codec::decoder& d, cached_map<K,V>& m);
template <class K, class V>
PN_CPP_EXTERN proton::codec::encoder& operator<<(proton::codec::encoder& e, const cached_map<K,V>& m);

template <class key_type, class value_type>
class PN_CPP_CLASS_EXTERN cached_map {
    typedef map_type_impl<key_type, value_type> map_type;

  public:
    PN_CPP_EXTERN cached_map();
    PN_CPP_EXTERN cached_map(const cached_map& cm);
    PN_CPP_EXTERN cached_map& operator=(const cached_map& cm);
#if PN_CPP_HAS_RVALUE_REFERENCES
    PN_CPP_EXTERN cached_map(cached_map&&);
    PN_CPP_EXTERN cached_map& operator=(cached_map&&);
#endif
    PN_CPP_EXTERN ~cached_map();

    PN_CPP_EXTERN value_type get(const key_type& k) const;
    PN_CPP_EXTERN void put(const key_type& k, const value_type& v);
    PN_CPP_EXTERN size_t erase(const key_type& k);
    PN_CPP_EXTERN bool exists(const key_type& k) const;
    PN_CPP_EXTERN size_t size();
    PN_CPP_EXTERN void clear();
    PN_CPP_EXTERN bool empty();

  /// @cond INTERNAL
  private:
    pn_unique_ptr<map_type> map_;
 
    void make_cached_map();

  friend PN_CPP_EXTERN proton::codec::decoder& operator>> <>(proton::codec::decoder& d, cached_map<key_type, value_type>& m);
  friend PN_CPP_EXTERN proton::codec::encoder& operator<< <>(proton::codec::encoder& e, const cached_map<key_type, value_type>& m);
  /// @endcond
};


}
}

#endif // PROTON_CPP_CACHED_MAP_H
