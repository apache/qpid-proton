#ifndef PROTON_CPP_PROTONIMPL_H
#define PROTON_CPP_PROTONIMPL_H

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

#include "proton/export.hpp"
#include "proton/object.h"

namespace proton {

// Modified from qpid::messaging version to work without
// boost::intrusive_ptr but integrate with Proton's pn_class_t
// reference counting.  Thread safety currently absent but fluid in
// intention...


/**
 * See private_impl_ref.h  This is for lightly wrapped Proton pn_object_t targets.
 * class Foo : proton_handle<pn_foo_t> {...}
 */

template <class T> class proton_impl_ref {
  public:
    typedef typename T::Impl Impl;

    /** Get the implementation pointer from a handle */
    static Impl* get(const T& t) { return t.impl_; }

    /** Set the implementation pointer in a handle */
    static void set(T& t, const Impl* p) {
        if (t.impl_ == p) return;
        if (t.impl_) pn_decref(t.impl_);
        t.impl_ = const_cast<Impl *>(p);
        if (t.impl_) pn_incref(t.impl_);
    }

    // Helper functions to implement the ctor, dtor, copy, assign
    static void ctor(T& t, Impl* p) { t.impl_ = p; if (p) pn_incref(p); }
    static void copy(T& t, const T& x) { if (&t == &x) return; t.impl_ = 0; assign(t, x); }
    static void dtor(T& t) { if(t.impl_) pn_decref(t.impl_); }
    static T& assign(T& t, const T& x) { set(t, get(x)); return t;}
};

}

#endif  /*!PROTON_CPP_PROTONIMPL_H*/
