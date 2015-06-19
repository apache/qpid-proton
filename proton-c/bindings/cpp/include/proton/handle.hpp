#ifndef PROTON_CPP_HANDLE_H
#define PROTON_CPP_HANDLE_H

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

namespace proton {

template <class> class private_impl_ref;
template <class> class proton_impl_ref;

// FIXME aconway 2015-06-09: don't need handle, get rid of it.

/**
 * A handle is like a pointer: refers to an underlying implementation object.
 * Copying the handle does not copy the object.
 *
 * Handles can be null,  like a 0 pointer. Use is_valid(), is_null() or the
 * conversion to bool to test for a null handle.
 */
template <class T> class handle {
  public:

    /**@return true if handle is valid,  i.e. not null. */
    bool is_valid() const { return impl_; }

    /**@return true if handle is null. It is an error to call any function on a null handle. */
    bool is_null() const { return !impl_; }

    /** Conversion to bool supports idiom if (handle) { handle->... } */
    operator bool() const { return impl_; }

    /** Operator ! supports idiom if (!handle) { do_if_handle_is_null(); } */
    bool operator !() const { return !impl_; }

    /** Operator ==  equal if they point to same non-null object*/
    bool operator ==(const handle<T>& other) const { return impl_ == other.impl_; }
    bool operator !=(const handle<T>& other) const { return impl_ != other.impl_; }

    void swap(handle<T>& h) { T* t = h.impl_; h.impl_ = impl_; impl_ = t; }

  private:
    // Not implemented, subclasses must implement.
    handle(const handle&);
    handle& operator=(const handle&);

  protected:
    typedef T Impl;
    handle() : impl_() {}

    mutable Impl* impl_;

  friend class private_impl_ref<T>;
};

}

#endif  /*!PROTON_CPP_HANDLE_H*/
