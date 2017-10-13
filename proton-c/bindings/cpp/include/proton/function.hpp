#ifndef PROTON_FUNCTION_HPP
#define PROTON_FUNCTION_HPP

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

/// @file
/// **Deprecated** - Use the API in `work_queue.hpp`.

/// @cond INTERNAL

namespace proton {

/// **Deprecated** - Use `proton::work`.
///
/// A C++03-compatible void no-argument callback function object.
///
/// Used by `container::schedule()` and `work_queue::add()`.  In C++11
/// you can use `std::bind`, `std::function`, or a void-no-argument
/// lambda instead.
///
/// `void_function0` is passed by reference, so instances of
/// subclasses do not have to be heap allocated.  Once passed, the
/// instance must not be deleted until its `operator()` is called or
/// the container has stopped.
class void_function0 {
  public:
    virtual ~void_function0() {}
    /// Override the call operator with your code.
    virtual void operator()() = 0;
};

} // proton

/// @endcond

#endif // PROTON_FUNCTION_HPP
