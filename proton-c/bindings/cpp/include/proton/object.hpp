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

#include "proton/config.hpp"
#include "proton/export.hpp"

#include <memory>

namespace proton {

/**
 * Base class for proton object types
 *
 * Automatically perform memory management for pn_object based types.
 *
 * Note that for the memory management to work correctly no class
 * inheriting from this should define any data members of its own.
 * 
 * This is ok because the entire purpose of this class is to provide
 * and manage the underlying pointer to the proton-c object.
 * 
 * So any class using this one needs to be merely a thin wrapping of
 * the proton-c object with no data members of it's own. Anything that internally
 * needs extra data members and a proton-c object must use an inheritor of
 * object<> as a data member - it must not inherit and add data members.
 * 
 */
class object_base {
  public:
    object_base() : object_(0) {}
    object_base(void* o) : object_(o) {}
    object_base(const object_base& o) : object_(o.object_) { incref(); }

    #ifdef PN_HAS_CPP11
    object_base(object_base&& o) : object_base() { *this = o; }
    #endif

    ~object_base() { decref(); };

    object_base& operator=(object_base o)
    { std::swap(object_, o.object_); return *this; }

    bool operator!() const { return !object_; }

  private:
    PN_CPP_EXTERN void incref() const;
    PN_CPP_EXTERN void decref() const;

    void* object_;

  template <class T>
  friend class object;
  template <class T>
  friend class owned_object;
  friend bool operator==(const object_base&, const object_base&);
};

template <class T>
class owned_object : public object_base {
  public:
    owned_object(T* o) : object_base(o) {};

  protected:
    T* pn_object() const { return static_cast<T*>(object_); }

  template <class U>
  friend class object;
};

template <class T>
class object : public object_base {
  public:
    object(T* o, bool take_own=false) : object_base(o) { if (!take_own) incref(); };
    object(owned_object<T> o) : object_base(o.object_) { o.object_=0; };

    object& operator=(owned_object<T> o)
    { object_ = o.object_; o = 0; return *this; }

  protected:
    static const bool take_ownership = true;
    T* pn_object() const { return static_cast<T*>(object_); }
};

inline
bool operator==(const object_base& o1, const object_base& o2) {
    return o1.object_==o2.object_;
}

inline
bool operator!=(const object_base& o1, const object_base& o2) {
    return !(o1==o2);
}

}
#endif // OBJECT_HPP
