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

#include "proton/map.hpp"

#include "proton/annotation_key.hpp"
#include "proton/scalar.hpp"
#include "proton/value.hpp"
#include "proton/codec/decoder.hpp"
#include "proton/codec/encoder.hpp"
#include "proton/codec/map.hpp"

#include <map>
#include <string>

// IMPLEMENTATION NOTES:
// - if (map_.get()) then *map_ is the authority and value_ is empty()
// - cache() ensures that *map_ is up to date and value_ is cleared.
// - flush() ensures value_ is up to date and map_ is cleared.

namespace proton {

// use std::map as the actual map implementation type
template <class K, class T>
class map_type_impl : public std::map<K, T> {};

template <class K, class T>
map<K,T>::map() {}

template <class K, class T>
map<K,T>::map(const map& x) { *this = x; }

template <class K, class T>
map<K,T>::map(pn_data_t *d) : value_(d) {
    // NOTE: for internal use. Don't verify that the data is valid here as that
    // would forcibly decode message maps immediately, we want to decode on-demand.
}

template <class K, class T>
PN_CPP_EXTERN void swap(map<K,T>& x, map<K,T>& y) {
    using namespace std;
    swap(x.map_, y.map_);
    swap(x.value_, y.value_);
}

template <class K, class T>
map<K,T>& map<K,T>::operator=(const map& x) {
    if (&x != this) {
        map_.reset(x.map_.get() ? new map_type(*x.map_) : 0);
        value_ = x.value_;
    }
    return *this;
}

#if PN_CPP_HAS_RVALUE_REFERENCES
template <class K, class T>
map<K,T>::map(map&& x) :
    map_(std::move(x.map_)), value_(std::move(x.value_)) {}

template <class K, class T>
map<K,T>& map<K,T>::operator=(map&& x) {
    if (&x != this) {
        map_.reset(x.map_.release());
        value_ = std::move(x.value_);
    }
    return *this;
}
#endif

template <class K, class T>
map<K,T>::~map() {}

// Make sure map_ is valid
template <class K, class T>
typename map<K,T>::map_type& map<K,T>::cache() const {
    if (!map_) {
        map_.reset(new map_type);
        if (!value_.empty()) {
            proton::get(value_, *map_);
            value_.clear();
        }
    }
    return *map_;
}

// Make sure value_ is valid
template <class K, class T>
value& map<K,T>::flush() const {
    if (map_.get()) {
        value_ = *map_;
        map_.reset();
    } else if (value_.empty()) {
        // Must contain an empty map, not be an empty value.
        codec::encoder(value_) << codec::start::map() << codec::finish();
    }
    return value_;
}

template <class K, class T>
void map<K,T>::value(const proton::value& x) {
    if (x.empty()) {
        clear();
    } else {
        internal::pn_unique_ptr<map_type> tmp(new map_type);
        proton::get(x, *tmp);  // Validate by decoding, may throw
        map_.reset(tmp.release());
        value_.clear();
    }
}

template <class K, class T>
proton::value& map<K,T>::value() { return flush(); }

template <class K, class T>
const proton::value& map<K,T>::value() const { return flush(); }

template <class K, class T>
T map<K,T>::get(const K& k) const {
    if (this->empty()) return T();
    typename map_type::const_iterator i = cache().find(k);
    if (i == map_->end()) return T();
    return i->second;
}

template <class K, class T>
void map<K,T>::put(const K& k, const T& v) {
    cache()[k] = v;
}

template <class K, class T>
size_t map<K,T>::erase(const K& k) {
    if (this->empty()) {
        return 0;
    } else {
        return cache().erase(k);
    }
}

template <class K, class T>
bool map<K,T>::exists(const K& k) const {
    return this->empty() ? 0 : cache().find(k) != cache().end();
}

template <class K, class T>
size_t map<K,T>::size() const {
    return this->empty() ? 0 : cache().size();
}

template <class K, class T>
void map<K,T>::clear() {
    map_.reset();               // Must invalidate the cache on clear()
    value_.clear();
}

template <class K, class T>
bool map<K,T>::empty() const {
    if (map_.get()) {
        return map_->empty();
    }
    if (value_.empty()) {
        return true;
    }
    // We must decode the non-empty value to see if it is an empty map.
    return cache().empty();
}

// Point to a different underlying pn_data_t, no copy
template <class K, class T>
void map<K,T>::reset(pn_data_t *d) {
    value_.reset(d);            // Points to d, not copy of d.
    map_.reset();
    // NOTE: for internal use. Don't verify that the data is valid here as that
    // would forcibly decode message maps immediately, we want to decode on-demand.
}

template <class K, class T>
PN_CPP_EXTERN proton::codec::decoder& operator>>(proton::codec::decoder& d, map<K,T>& m)
{
    m.map_.reset();
    d >> m.value_;
    m.cache();                  // Validate the value
    return d;
}

template <class K, class T>
PN_CPP_EXTERN proton::codec::encoder& operator<<(proton::codec::encoder& e, const map<K,T>& m)
{
    return e << m.value();   // Copy the value
}

// Force the necessary template instantiations so that the library exports the correct symbols
template class PN_CPP_CLASS_EXTERN map<std::string, scalar>;
typedef map<std::string, scalar> cm1;
template PN_CPP_EXTERN void swap<>(cm1&, cm1&);
template PN_CPP_EXTERN proton::codec::decoder& operator>> <>(proton::codec::decoder& d, cm1& m);
template PN_CPP_EXTERN proton::codec::encoder& operator<< <>(proton::codec::encoder& e, const cm1& m);

template class PN_CPP_CLASS_EXTERN map<annotation_key, value>;
typedef map<annotation_key, value> cm2;
template PN_CPP_EXTERN void swap<>(cm2&, cm2&);
template PN_CPP_EXTERN proton::codec::decoder& operator>> <>(proton::codec::decoder& d, cm2& m);
template PN_CPP_EXTERN proton::codec::encoder& operator<< <>(proton::codec::encoder& e, const cm2& m);

template class PN_CPP_CLASS_EXTERN map<symbol, value>;
typedef map<symbol, value> cm3;
template PN_CPP_EXTERN void swap<>(cm3&, cm3&);
template PN_CPP_EXTERN proton::codec::decoder& operator>> <>(proton::codec::decoder& d, cm3& m);
template PN_CPP_EXTERN proton::codec::encoder& operator<< <>(proton::codec::encoder& e, const cm3& m);

} // namespace proton
