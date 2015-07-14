#ifndef PROTON_CPP_PROTONHANDLE_H
#define PROTON_CPP_PROTONHANDLE_H

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

///@cond INTERNAL
template <class> class proton_impl_ref;
///@endcond INTERNAL

/**
 * See handle.h.  Similar but for lightly wrapped Proton pn_object_t targets.
 */
template <class T> class proton_handle {
  public:

    /**@return true if handle is valid,  i.e. not null. */
    bool is_valid() const { return impl_; }

    /**@return true if handle is null. It is an error to call any function on a null handle. */
    bool is_null() const { return !impl_; }

    /** Conversion to bool supports idiom if (handle) { handle->... } */
    operator bool() const { return impl_; }

    /** Operator ! supports idiom if (!handle) { do_if_handle_is_null(); } */
    bool operator !() const { return !impl_; }

    void swap(proton_handle<T>& h) { T* t = h.impl_; h.impl_ = impl_; impl_ = t; }

  private:
    // Not implemented, subclasses must implement.
    proton_handle(const proton_handle&);
    proton_handle& operator=(const proton_handle&);

  protected:
    typedef T Impl;
    proton_handle() : impl_() {}

    mutable Impl* impl_;

  friend class proton_impl_ref<T>;
};

}

#endif  /*!PROTON_CPP_PROTONHANDLE_H*/
