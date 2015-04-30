#ifndef PROTON_CPP_PRIVATEIMPL_H
#define PROTON_CPP_PRIVATEIMPL_H

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

#include "proton/cpp/ImportExport.h"

namespace proton {
namespace reactor {

// Modified from qpid::messaging version to work without
// boost::intrusive_ptr but integrate with Proton's pn_class_t
// reference counting.  Thread safety currently absent but fluid in
// intention...


/**
 * Helper class to implement a class with a private, reference counted
 * implementation and reference semantics.
 *
 * Such classes are used in the public API to hide implementation, they
 * should. Example of use:
 *
 * === Foo.h
 *
 * template <class T> PrivateImplRef;
 * class FooImpl;
 *
 * Foo : public Handle<FooImpl> {
 *  public:
 *   Foo(FooImpl* = 0);
 *   Foo(const Foo&);
 *   ~Foo();
 *   Foo& operator=(const Foo&);
 *
 *   int fooDo();              //  and other Foo functions...
 *
 *  private:
 *   typedef FooImpl Impl;
 *   Impl* impl;
 *   friend class PrivateImplRef<Foo>;
 *
 * === Foo.cpp
 *
 * typedef PrivateImplRef<Foo> PI;
 * Foo::Foo(FooImpl* p) { PI::ctor(*this, p); }
 * Foo::Foo(const Foo& c) : Handle<FooImpl>() { PI::copy(*this, c); }
 * Foo::~Foo() { PI::dtor(*this); }
 * Foo& Foo::operator=(const Foo& c) { return PI::assign(*this, c); }
 *
 * int foo::fooDo() { return impl->fooDo(); }
 *
 */
template <class T> class PrivateImplRef {
  public:
    typedef typename T::Impl Impl;

    /** Get the implementation pointer from a handle */
    static Impl* get(const T& t) { return t.impl; }

    /** Set the implementation pointer in a handle */
    static void set(T& t, const Impl* p) {
        if (t.impl == p) return;
        if (t.impl) Impl::decref(t.impl);
        t.impl = const_cast<Impl *>(p);
        if (t.impl) Impl::incref(t.impl);
    }

    // Helper functions to implement the ctor, dtor, copy, assign
    static void ctor(T& t, Impl* p) { t.impl = p; if (p) Impl::incref(p); }
    static void copy(T& t, const T& x) { if (&t == &x) return; t.impl = 0; assign(t, x); }
    static void dtor(T& t) { if(t.impl) Impl::decref(t.impl); }
    static T& assign(T& t, const T& x) { set(t, get(x)); return t;}
};

}} // namespace proton::reactor

#endif  /*!PROTON_CPP_PRIVATEIMPL_H*/
