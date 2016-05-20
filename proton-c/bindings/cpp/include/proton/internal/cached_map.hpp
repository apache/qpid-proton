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

#include "../value.hpp"
#include "../codec/decoder.hpp"
#include "../codec/encoder.hpp"

#include "./config.hpp"
#include "./pn_unique_ptr.hpp"

#include <map>

namespace proton {
namespace internal {

/// A convenience class to view and manage AMQP map data contained in
/// a proton::value.  An internal cache of the map data is created as
/// needed.  If desired, a std::map can be extracted from or inserted
/// into the cached_map value directly.
template <class key_type, class value_type>
class cached_map {
    typedef std::map<key_type, value_type> map_type;
  public:

#if PN_CPP_HAS_DEFAULTED_FUNCTIONS
    cached_map() = default;
    cached_map(const cached_map&) = default;
    cached_map& operator=(const cached_map&) = default;
    cached_map(cached_map&&) = default;
    cached_map& operator=(cached_map&&) = default;
    ~cached_map() = default;
#endif

    value_type get(const key_type& k) const {
      typename map_type::const_iterator i = map_.find(k);
      if ( i==map_.end() ) return value_type();
      return i->second;
    }
    void put(const key_type& k, const value_type& v) {
      map_[k] = v;
    }
    size_t erase(const key_type& k) {
      return map_.erase(k);
    }
    bool exists(const key_type& k) const {
      return map_.count(k) > 0;
    }

    size_t size() {
      return map_.size();
    }
    void clear() {
      map_.clear();
    }
    bool empty() {
      return map_.empty();
    }

    /// @cond INTERNAL
  private:
    map_type map_;
    /// @endcond

    friend proton::codec::decoder& operator>>(proton::codec::decoder& d, cached_map& m) {
      return d >> m.map_;
    }
    friend proton::codec::encoder& operator<<(proton::codec::encoder& e, const cached_map& m) {
      return e << m.map_;
    }
};


}
}

#endif // PROTON_CPP_CACHED_MAP_H
