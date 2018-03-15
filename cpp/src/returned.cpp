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

#include "proton_bits.hpp"

#include <proton/internal/export.hpp>

#include <proton/returned.hpp>
#include <proton/connection.hpp>
#include <proton/sender.hpp>
#include <proton/receiver.hpp>

namespace proton {

template <class T> PN_CPP_EXTERN returned<T>::returned(const returned<T>& x) : ptr_(x.ptr_) {}

template <class T> PN_CPP_EXTERN returned<T>::operator T() const {
    return internal::factory<T>::wrap(ptr_);
}

template <class T> returned<T>::returned(typename T::pn_type* p) : ptr_(p) {}

// Explicit instantiations for allowed types

template class PN_CPP_CLASS_EXTERN returned<connection>;
template class PN_CPP_CLASS_EXTERN returned<sender>;
template class PN_CPP_CLASS_EXTERN returned<receiver>;

}
