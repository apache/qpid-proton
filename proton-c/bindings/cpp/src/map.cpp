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
// - either value_, map_ or both can hold the up-to-date data
// - avoid encoding or decoding between map_ and value_ unless necessary
//   - if (map.get()) then map_ is up to date
//   - if (!value_.empty()) then value_ is up to date
//   - if (map.get_() && !value_.empty()) then map_ and value_ have equivalent data
//   - if (!map.get_() && value_.empty()) then that's equivalent to an empty map
// - cache() ensures that *map_ is up to date
// - flush() ensures value_ is up to date

namespace proton {

// use std::map as the actual map implementation type
template <class K, class T>
class map_type_impl : public std::map<K, T> {};

template <class K, class T>
map<K,T>::map() {}

template <class K, class T>
map<K,T>::map(const map& x) { *this = x; }

template <class K, class T>
map<K,T>::map(pn_data_t *d) : value_(d) {}

template <class K, class T>
PN_CPP_EXTERN void swap(map<K,T>& x, map<K,T>& y) {
    using namespace std;
    swap(x.map_, y.map_);
    swap(x.value_, y.value_);
}

template <class K, class T>
void map<K,T>::ensure() const {
    if (!map_) {
        map_.reset(new map<K,T>::map_type);
    }
}

template <class K, class T>
map<K,T>& map<K,T>::operator=(const map& x) {
    if (&x != this) {
        if (!x.value_.empty()) {
            map_.reset();
            value_ = x.value_;
        } else if (x.map_.get()) {
            value_.clear();
            ensure();
            *map_ = *x.map_;
        } else {
            clear();
        }
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
const typename map<K,T>::map_type& map<K,T>::cache() const {
    if (!map_) {
        ensure();
        if (!value_.empty()) {
            proton::get(value_, *map_);
        }
    }
    return *map_;
}

// Make sure map_ is valid, and mark value_ invalid
template <class K, class T>
typename map<K,T>::map_type& map<K,T>::cache_update() {
    cache();
    value_.clear();
    return *map_;
}

template <class K, class T>
value& map<K,T>::flush() const {
    if (value_.empty()) {
        // Create an empty map if need be, value_ must hold a valid map (even if empty)
        // it must not be an empty (NULL_TYPE) proton::value.
        ensure();
        value_ = *map_;
    }
    return value_;
}

template <class K, class T>
void map<K,T>::value(const proton::value& x) {
    value_.clear();
    // Validate the value by decoding it into map_, throw if not a valid map value.
    ensure();
    proton::get(x, *map_);
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
    cache_update()[k] = v;
}

template <class K, class T>
size_t map<K,T>::erase(const K& k) {
    if (this->empty()) {
        return 0;
    } else {
        return cache_update().erase(k);
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
    if (map_.get()) {
        map_->clear();
    }
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
    // Decode to m.map_ rather than m.value_ to verify the data is of valid type.
    m.value_.clear();
    m.ensure();
    d >> *m.map_;
    return d;
}

template <class K, class T>
PN_CPP_EXTERN proton::codec::encoder& operator<<(proton::codec::encoder& e, const map<K,T>& m)
{
    if (!m.value_.empty()) {
        return e << m.value_;   // Copy the value
    }
    // Encode the (possibly empty) map_.
    m.ensure();
    return e << *(m.map_);
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
