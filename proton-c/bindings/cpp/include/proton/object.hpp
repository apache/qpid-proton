#ifndef OBJECT_HPP
#define OBJECT_HPP

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

/// @cond INTERNAL

#include "proton/config.hpp"
#include "proton/export.hpp"
#include "proton/comparable.hpp"
#include <memory>

namespace proton {
namespace internal {

class pn_ptr_base {
  protected:
    PN_CPP_EXTERN static void incref(void* p);
    PN_CPP_EXTERN static void decref(void* p);
};

template <class T> class pn_ptr : private pn_ptr_base, private comparable<pn_ptr<T> > {
  public:
    pn_ptr() : ptr_(0) {}
    pn_ptr(T* p) : ptr_(p) { incref(ptr_); }
    pn_ptr(const pn_ptr& o) : ptr_(o.ptr_) { incref(ptr_); }

#if PN_CPP_HAS_CPP11
    pn_ptr(pn_ptr&& o) : ptr_(0) { std::swap(ptr_, o.ptr_); }
#endif

    ~pn_ptr() { decref(ptr_); }

    pn_ptr& operator=(pn_ptr o) { std::swap(ptr_, o.ptr_); return *this; }

    T* get() const { return ptr_; }
    T* release() { T *p = ptr_; ptr_ = 0; return p; }
    bool operator!() const { return !ptr_; }

    static pn_ptr take_ownership(T* p) { return pn_ptr<T>(p, true); }

  private:
    T *ptr_;

    // Note that it is the presence of the bool in the constructor signature that matters
    // to get the "transfer ownership" constructor: The value of the bool isn't checked.
    pn_ptr(T* p, bool) : ptr_(p) {}

    friend bool operator==(const pn_ptr& a, const pn_ptr& b) { return a.ptr_ == b.ptr_; }
    friend bool operator<(const pn_ptr& a, const pn_ptr& b) { return a.ptr_ < b.ptr_; }
};

template <class T> pn_ptr<T> take_ownership(T* p) { return pn_ptr<T>::take_ownership(p); }

/// Base class for proton object types.
template <class T> class object : private comparable<object<T> > {
  public:
    bool operator!() const { return !object_; }

  protected:
    object(pn_ptr<T> o) : object_(o) {}
    T* pn_object() const { return object_.get(); }

  private:
    pn_ptr<T> object_;

    friend bool operator==(const object& a, const object& b) { return a.object_ == b.object_; }
    friend bool operator<(const object& a, const object& b) { return a.object_ < b.object_; }
};

}}

/// @endcond

#endif // OBJECT_HPP
