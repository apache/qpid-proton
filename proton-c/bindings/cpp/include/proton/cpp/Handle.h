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

#include "proton/cpp/ImportExport.h"

namespace proton {
namespace reactor {

template <class> class PrivateImplRef;

/**
 * A handle is like a pointer: refers to an underlying implementation object.
 * Copying the handle does not copy the object.
 *
 * Handles can be null,  like a 0 pointer. Use isValid(), isNull() or the
 * conversion to bool to test for a null handle.
 */
template <class T> class Handle {
  public:

    /**@return true if handle is valid,  i.e. not null. */
    PROTON_CPP_INLINE_EXTERN bool isValid() const { return impl; }

    /**@return true if handle is null. It is an error to call any function on a null handle. */
    PROTON_CPP_INLINE_EXTERN bool isNull() const { return !impl; }

    /** Conversion to bool supports idiom if (handle) { handle->... } */
    PROTON_CPP_INLINE_EXTERN operator bool() const { return impl; }

    /** Operator ! supports idiom if (!handle) { do_if_handle_is_null(); } */
    PROTON_CPP_INLINE_EXTERN bool operator !() const { return !impl; }

    void swap(Handle<T>& h) { T* t = h.impl; h.impl = impl; impl = t; }

  private:
    // Not implemented, subclasses must implement.
    Handle(const Handle&);
    Handle& operator=(const Handle&);

  protected:
    typedef T Impl;
    PROTON_CPP_INLINE_EXTERN Handle() :impl() {}

    Impl* impl;

  friend class PrivateImplRef<T>;
};

}} // namespace proton::reactor

#endif  /*!PROTON_CPP_HANDLE_H*/
