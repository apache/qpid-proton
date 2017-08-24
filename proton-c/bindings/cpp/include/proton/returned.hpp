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

#include "./internal/object.hpp"
#include "./connection.hpp"
#include "./receiver.hpp"
#include "./sender.hpp"

/// @file
/// Return type for container functions

namespace proton {

namespace internal {
template <class T> class factory;
}

/// Return type for container functions
///
/// @note returned value is *thread-unsafe*.
/// A single-threaded application can assign the returned<T> value to a plain T.
/// A multi-threaded application *must* ignore the returned value, as it may already
/// be invalid by the time the function returns. Multi-threaded applications
/// can access the value in @ref messaging_handler functions.
///
template <class T>
class returned
{
  public:
    operator T() const;

  private:
    typename T::pn_type* ptr_;
    returned(const T&);
    returned& operator=(const returned&);
    template <class U> friend class internal::factory;
};

} // proton

#endif  /*!PROTON_RETURNED_HPP*/
