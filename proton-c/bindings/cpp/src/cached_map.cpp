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

#include "proton/internal/cached_map.hpp"

#include "proton/annotation_key.hpp"
#include "proton/scalar.hpp"
#include "proton/value.hpp"
#include "proton/codec/decoder.hpp"
#include "proton/codec/encoder.hpp"
#include "proton/codec/map.hpp"

#include <map>
#include <string>

namespace proton {
namespace internal {

 // use std::map as the actual cached_map implementation type
template <class K, class V>
class map_type_impl : public std::map<K, V> {};

template <class K, class V>
cached_map<K,V>::cached_map() {}
template <class K, class V>
cached_map<K,V>::cached_map(const cached_map& cm) { if ( !cm.map_ ) return;  map_.reset(new map_type(*cm.map_)); }
template <class K, class V>
cached_map<K,V>& cached_map<K,V>::operator=(const cached_map& cm) {
    if (&cm != this) {
        cached_map<K,V> t;
        map_type *m = !cm.map_ ? 0 : new map_type(*cm.map_);
        t.map_.reset(map_.release());
        map_.reset(m);
    }
    return *this;
}

template <class K, class V>
#if PN_CPP_HAS_RVALUE_REFERENCES
cached_map<K,V>::cached_map(cached_map&& cm) : map_(std::move(cm.map_)) {}
template <class K, class V>
cached_map<K,V>& cached_map<K,V>::operator=(cached_map&& cm) { map_.reset(cm.map_.release()); return *this; }
template <class K, class V>
#endif
cached_map<K,V>::~cached_map() {}

template <class K, class V>
V cached_map<K,V>::get(const K& k) const {
  if ( !map_ ) return V();
  typename map_type::const_iterator i = map_->find(k);
  if ( i==map_->end() ) return V();
  return i->second;
}
template <class K, class V>
void cached_map<K,V>::put(const K& k, const V& v) {
  if ( !map_ ) make_cached_map();
  (*map_)[k] = v;
}
template <class K, class V>
size_t cached_map<K,V>::erase(const K& k) {
  if ( !map_ ) return 0;
  return map_->erase(k);
}
template <class K, class V>
bool cached_map<K,V>::exists(const K& k) const {
  if ( !map_ ) return false;
  return map_->count(k) > 0;
}

template <class K, class V>
size_t cached_map<K,V>::size() {
  if ( !map_ ) return 0;
  return map_->size();
}
template <class K, class V>
void cached_map<K,V>::clear() {
  map_.reset();
}
template <class K, class V>
bool cached_map<K,V>::empty() {
  if ( !map_ ) return true;
  return map_->empty();
}

template <class K, class V>
void cached_map<K,V>::make_cached_map() { map_.reset(new map_type); }

template <class K, class V>
PN_CPP_EXTERN proton::codec::decoder& operator>>(proton::codec::decoder& d, cached_map<K,V>& m) {
  if ( !m.map_ ) m.make_cached_map();
  return d >> *(m.map_);
}
template <class K, class V>
PN_CPP_EXTERN proton::codec::encoder& operator<<(proton::codec::encoder& e, const cached_map<K,V>& m) {
  if ( !m.map_ ) return e;
  return e << *(m.map_);
}

// Force the necessary template instantiations so that the library exports the correct symbols
template class PN_CPP_CLASS_EXTERN cached_map<std::string, scalar>;
template class PN_CPP_CLASS_EXTERN cached_map<annotation_key, value>;
template class PN_CPP_CLASS_EXTERN cached_map<symbol, value>;

template proton::codec::decoder& operator>> <>(proton::codec::decoder& d, cached_map<std::string, scalar>& m);
template proton::codec::encoder& operator<< <>(proton::codec::encoder& e, const cached_map<std::string, scalar>& m);
template proton::codec::decoder& operator>> <>(proton::codec::decoder& d, cached_map<annotation_key, value>& m);
template proton::codec::encoder& operator<< <>(proton::codec::encoder& e, const cached_map<annotation_key, value>& m);
template proton::codec::decoder& operator>> <>(proton::codec::decoder& d, cached_map<symbol, value>& m);
template proton::codec::encoder& operator<< <>(proton::codec::encoder& e, const cached_map<symbol, value>& m);

}
}
