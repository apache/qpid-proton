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
#include "proton/error_condition.hpp"
#include "proton/comparable.hpp"

namespace proton {

/// The base class for session, connection, and link.
class
PN_CPP_CLASS_EXTERN endpoint {
  public:
    PN_CPP_EXTERN virtual ~endpoint();

    /// True if the local end is uninitialized
    virtual bool uninitialized() const = 0;
    /// True if the local end is active
    virtual bool active() const = 0;
    /// True if the connection is fully closed, i.e. local and remote
    /// ends are closed.
    virtual bool closed() const = 0;

    /// Get the error condition of the remote endpoint.
    virtual class error_condition error() const = 0;

    /// Close endpoint
    virtual void close() = 0;
    virtual void close(const error_condition&) = 0;

#if PN_CPP_HAS_DEFAULTED_FUNCTIONS
    // Make everything explicit for C++11 compilers
    endpoint() = default;
    endpoint& operator=(const endpoint&) = default;
    endpoint& operator=(endpoint&&) = default;

    endpoint(const endpoint&) = default;
    endpoint(endpoint&&) = default;
#endif
};

///@cond INTERNAL
namespace internal {

template <class T, class D> class iter_base {
  public:
    typedef T value_type;

    T operator*() const { return obj_; }
    T* operator->() const { return const_cast<T*>(&obj_); }
    D operator++(int) { D x(*this); ++(*this); return x; }
    bool operator==(const iter_base<T, D>& x) const { return obj_ == x.obj_; }
    bool operator!=(const iter_base<T, D>& x) const { return obj_ != x.obj_; }
    ///@}
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
///@endcond

} // namespace proton

#endif // PROTON_CPP_H
