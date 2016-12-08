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

#include "./fwd.hpp"
#include "./internal/config.hpp"
#include "./internal/export.hpp"
#include "./internal/pn_unique_ptr.hpp"

#include <functional>

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
    /// @cond internal
    class impl;
    event_loop& operator=(impl* i);
    /// @endcond

  public:
    /// Create event_loop
    PN_CPP_EXTERN event_loop();

    PN_CPP_EXTERN ~event_loop();

#if PN_CPP_HAS_EXPLICIT_CONVERSIONS
    /// When using C++11 (or later) you can use event_loop in a bool context
    /// to indicate if there is an event loop set.
    PN_CPP_EXTERN explicit operator bool() const { return bool(impl_); }
#endif

    /// No event loop set.
    PN_CPP_EXTERN bool operator !() const { return !impl_; }

    /// Arrange to have f() called in the event_loop's sequence: possibly
    /// deferred, possibly in another thread.
    ///
    /// @return true if f() has or will be called, false if the event_loop is ended
    /// and f() cannot be injected.
    PN_CPP_EXTERN bool inject(void_function0& f);

#if PN_CPP_HAS_STD_FUNCTION
    /// @copydoc inject(void_function0&)
    PN_CPP_EXTERN bool inject(std::function<void()> f);
#endif

  private:
    PN_CPP_EXTERN static event_loop& get(pn_connection_t*);
    PN_CPP_EXTERN static event_loop& get(pn_session_t*);
    PN_CPP_EXTERN static event_loop& get(pn_link_t*);

    internal::pn_unique_ptr<impl> impl_;

    /// @cond INTERNAL
  friend class container;
  friend class io::connection_driver;
  template <class T> friend class thread_safe;
    /// @endcond
};

} // proton

#endif // PROTON_EVENT_LOOP_HPP
