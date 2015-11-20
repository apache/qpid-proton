#ifndef COUNTED_PTR_HPP
#define COUNTED_PTR_HPP
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

#include "proton/config.hpp"
#include "proton/comparable.hpp"

#if PN_HAS_BOOST
#include <boost/shared_ptr.hpp>
#include <boost/intrusive_ptr.hpp>
#endif

#include <memory>

namespace proton {

///@cond INTERNAL

// Default refcounting uses pn_incref, pn_decref. Other types must define
// their own incref/decref overloads.
PN_CPP_EXTERN void incref(const void*);
PN_CPP_EXTERN void decref(const void*);

///@endcond

/**
 * Smart pointer for reference counted objects derived from `proton::counted`
 * or `proton::pn_counted`
 */
template <class T> class counted_ptr : public proton::comparable<counted_ptr<T> > {
  public:
    typedef T element_type;

    explicit counted_ptr(T *p = 0, bool add_ref = true) : ptr_(p) {
        if (add_ref) incref(ptr_);
    }

    counted_ptr(const counted_ptr<T>& p) : ptr_(p.ptr_) { incref(ptr_); }

    // TODO aconway 2015-08-20: C++11 move constructor

    ~counted_ptr() { decref(ptr_); }

    void swap(counted_ptr& x) { std::swap(ptr_, x.ptr_); }

    counted_ptr<T>& operator=(const counted_ptr<T>& p) {
        counted_ptr<T>(p.get()).swap(*this);
        return *this;
    }

    void reset(T* p=0, bool add_ref = true) {
        counted_ptr<T>(p, add_ref).swap(*this);
    }

    T* release() {
        T* ret = ptr_;
        ptr_ = 0;
        return ret;
    }

    T* get() const { return ptr_; }
    T* operator->() const { return ptr_; }
    T& operator*() const { return *ptr_; }
    operator bool() const { return !!ptr_; }
    bool operator!() const { return !ptr_; }

    template <class U> operator counted_ptr<U>() const { return counted_ptr<U>(get()); }
    template <class U> bool operator==(const counted_ptr<U>& x) { return get() == x.get(); }
    template <class U> bool operator<(const counted_ptr<U>& x) { return get() < x.get(); }

#if PN_HAS_STD_PTR
    // TODO aconway 2015-08-21: need weak pointer context for efficient shared_ptr
    operator std::shared_ptr<T>() { return std::shared_ptr<T>(dup()); }
    operator std::shared_ptr<const T>() const { return std::shared_ptr<const T>(dup()); }
    operator std::unique_ptr<T>() { return std::unique_ptr<T>(dup()); }
    operator std::unique_ptr<const T>() const { return std::unique_ptr<const T>(dup()); }
#endif
#if PN_HAS_BOOST
    // TODO aconway 2015-08-21: need weak pointer context for efficient shared_ptr
    operator boost::shared_ptr<T>() { return boost::shared_ptr<T>(dup()); }
    operator boost::shared_ptr<const T>() const { return boost::shared_ptr<const T>(dup()); }
    operator boost::intrusive_ptr<T>() { return boost::intrusive_ptr<T>(ptr_); }
    operator boost::intrusive_ptr<const T>() const { return boost::intrusive_ptr<const T>(ptr_); }
#endif

  private:
    T* dup() { incref(ptr_); return ptr_; }
    const T* dup() const { incref(ptr_); return ptr_; }

    T* ptr_;
};

#if PN_HAS_BOOST
template <class T> inline void intrusive_ptr_add_ref(const T* p) { if (p) incref(p); }
template <class T> inline void intrusive_ptr_release(const T* p) { if (p) decref(p); }
#endif

}

#endif // COUNTED_PTR_HPP
