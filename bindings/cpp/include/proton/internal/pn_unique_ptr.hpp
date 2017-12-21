#ifndef PROTON_INTERNAL_UNIQUE_PTR_HPP
#define PROTON_INTERNAL_UNIQUE_PTR_HPP

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

#include <memory>

namespace proton {
namespace internal {

template <class T> class pn_unique_ptr;
template <class T> void swap(pn_unique_ptr<T>& x, pn_unique_ptr<T>& y);

/// A simple unique ownership pointer, used as a return value from
/// functions that transfer ownership to the caller.
///
/// pn_unique_ptr return values should be converted immediately to
/// std::unique_ptr if that is available or std::auto_ptr (by calling
/// release()) for older C++. You should not use pn_unique_ptr in your
/// own code.  It is a limited pointer class designed only to work
/// around differences between C++11 and C++03.
template <class T> class pn_unique_ptr {
  public:
    pn_unique_ptr(T* p=0) : ptr_(p) {}
#if PN_CPP_HAS_RVALUE_REFERENCES
    pn_unique_ptr(pn_unique_ptr&& x) : ptr_(0)  { std::swap(ptr_, x.ptr_); }
#else
    pn_unique_ptr(const pn_unique_ptr& x) : ptr_() { std::swap(ptr_, const_cast<pn_unique_ptr&>(x).ptr_); }
#endif
    ~pn_unique_ptr() { delete(ptr_); }
    T& operator*() const { return *ptr_; }
    T* operator->() const { return ptr_; }
    T* get() const { return ptr_; }
    void reset(T* p = 0) { pn_unique_ptr<T> tmp(p); std::swap(ptr_, tmp.ptr_); }
    T* release() { T *p = ptr_; ptr_ = 0; return p; }
#if PN_CPP_HAS_EXPLICIT_CONVERSIONS
    explicit operator bool() const { return get(); }
#endif
    bool operator !() const { return !get(); }
    void swap(pn_unique_ptr& x) { std::swap(ptr_, x.ptr_); }

#if PN_CPP_HAS_STD_UNIQUE_PTR
    operator std::unique_ptr<T>() { T *p = ptr_; ptr_ = 0; return std::unique_ptr<T>(p); }
#endif

  private:
    T* ptr_;
};

template <class T> void swap(pn_unique_ptr<T>& x, pn_unique_ptr<T>& y) { x.swap(y); }

} // internal
} // proton

#endif // PROTON_INTERNAL_UNIQUE_PTR_HPP
