#ifndef PROTON_CPP_ENDPOINT_H
#define PROTON_CPP_ENDPOINT_H

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
#include "proton/config.hpp"
#include "proton/export.hpp"
#include "proton/condition.hpp"
#include "proton/comparable.hpp"

namespace proton {

/// The base class for session, connection, and link.
class
PN_CPP_CLASS_EXTERN endpoint {
  public:
    PN_CPP_EXTERN virtual ~endpoint();

    /// A bit mask of state bit values.
    ///
    /// A state mask is matched against an endpoint as follows: If the
    /// state mask contains both local and remote flags, then an exact
    /// match against those flags is performed. If state contains only
    /// local or only remote flags, then a match occurs if any of the
    /// local or remote flags are set respectively.
    ///
    /// @see connection::links, connection::sessions
    typedef int state;

    // XXX use an enum instead to handle name collision

    PN_CPP_EXTERN static const state LOCAL_UNINIT;  ///< Local endpoint is uninitialized
    PN_CPP_EXTERN static const state REMOTE_UNINIT; ///< Remote endpoint is uninitialized
    PN_CPP_EXTERN static const state LOCAL_ACTIVE;  ///< Local endpoint is active
    PN_CPP_EXTERN static const state REMOTE_ACTIVE; ///< Remote endpoint is active
    PN_CPP_EXTERN static const state LOCAL_CLOSED;  ///< Local endpoint has been closed
    PN_CPP_EXTERN static const state REMOTE_CLOSED; ///< Remote endpoint has been closed
    PN_CPP_EXTERN static const state LOCAL_MASK;    ///< Mask including all LOCAL_ bits (UNINIT, ACTIVE, CLOSED)
    PN_CPP_EXTERN static const state REMOTE_MASK;   ///< Mask including all REMOTE_ bits (UNINIT, ACTIVE, CLOSED)

    /// XXX add endpoint state boolean operations

    /// Get the local error condition.
    virtual condition local_condition() const = 0;

    /// Get the error condition of the remote endpoint.
    virtual condition remote_condition() const = 0;

#if PN_CPP_HAS_CPP11
    // Make everything explicit for C++11 compilers
    endpoint() = default;
    endpoint& operator=(const endpoint&) = default;
    endpoint& operator=(endpoint&&) = default;

    endpoint(const endpoint&) = default;
    endpoint(endpoint&&) = default;
#endif
};

namespace internal {

template <class T, class D> class iter_base {
  public:
    typedef T value_type;

    T operator*() const { return obj_; }
    T* operator->() const { return const_cast<T*>(&obj_); }
    D operator++(int) { D x(*this); ++(*this); return x; }
    bool operator==(const iter_base<T, D>& x) const { return obj_ == x.obj_; }
    bool operator!=(const iter_base<T, D>& x) const { return obj_ != x.obj_; }

  protected:
    explicit iter_base(T p = 0) : obj_(p) {}
    T obj_;
};

template<class I> class iter_range {
  public:
    typedef I iterator;

    explicit iter_range(I begin = I(), I end = I()) : begin_(begin), end_(end) {}
    I begin() const { return begin_; }
    I end() const { return end_; }
    bool empty() const { return begin_ == end_; }
  private:
    I begin_, end_;
};

} // namespace internal
} // namespace proton

#endif // PROTON_CPP_H
