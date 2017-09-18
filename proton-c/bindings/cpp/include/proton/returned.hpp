#ifndef PROTON_RETURNED_HPP
#define PROTON_RETURNED_HPP

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

#include "./internal/export.hpp"
#include "./internal/object.hpp"

#include "./connection.hpp"
#include "./receiver.hpp"
#include "./sender.hpp"

/// @file
/// @copybrief proton::returned

namespace proton {

namespace internal {
class returned_factory;
}

/// A return type for container methods.
///
/// **Thread safety** - Container method return values are
/// *thread-unsafe*.  A single-threaded application can safely assign
/// the `returned<T>` value to a plain `T`.  A multithreaded
/// application *must* ignore the returned value because it may
/// already be invalid by the time the function returns.
/// Multithreaded applications can safely access the value inside @ref
/// messaging_handler functions.
template <class T>
class PN_CPP_CLASS_EXTERN returned {
  public:
    /// Copy operator required to return a value
    PN_CPP_EXTERN returned(const returned<T>&);

    /// Convert to the proton::object
    ///
    /// @note **Thread-unsafe** - Do not use in a multithreaded application.
    PN_CPP_EXTERN operator T() const;

  private:
    typename T::pn_type* ptr_;
    returned(typename T::pn_type*);
    returned& operator=(const returned&); // Not defined
  friend class internal::returned_factory;
};

} // proton

#endif  /*!PROTON_RETURNED_HPP*/
