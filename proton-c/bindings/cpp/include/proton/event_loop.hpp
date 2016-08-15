#ifndef PROTON_EVENT_LOOP_HPP
#define PROTON_EVENT_LOOP_HPP

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

#include "./internal/config.hpp"
#include "./function.hpp"

#include <functional>

#if PN_CPP_HAS_CPP11
#include <future>
#include <type_traits>
#endif

struct pn_connection_t;
struct pn_session_t;
struct pn_link_t;

namespace proton {

/// **Experimental** - A serial execution context.
///
/// Event handler functions associated with a single proton::connection are called in sequence.
/// The connection's @ref event_loop allows you to "inject" extra work from any thread,
/// and have it executed in the same sequence.
///
class PN_CPP_CLASS_EXTERN event_loop {
  public:
    virtual ~event_loop() {}

    /// Arrange to have f() called in the event_loop's sequence: possibly
    /// deferred, possibly in another thread.
    ///
    /// @return true if f() has or will be called, false if the event_loop is ended
    /// and f() cannot be injected.
    virtual bool inject(void_function0& f) = 0;

#if PN_CPP_HAS_STD_FUNCTION
    /// @copydoc inject(void_function0&)
    virtual bool inject(std::function<void()> f) = 0;
#endif

  protected:
    event_loop() {}

  private:
    PN_CPP_EXTERN static event_loop* get(pn_connection_t*);
    PN_CPP_EXTERN static event_loop* get(pn_session_t*);
    PN_CPP_EXTERN static event_loop* get(pn_link_t*);

    /// @cond INTERNAL
  friend class connection;
  template <class T> friend class thread_safe;
    /// @endcond
};

} // proton

#endif // PROTON_EVENT_LOOP_HPP
