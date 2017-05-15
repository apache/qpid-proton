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
#include "./function.hpp"
#include "./internal/config.hpp"
#include "./internal/export.hpp"
#include "./internal/pn_unique_ptr.hpp"

#include <functional>

struct pn_connection_t;
struct pn_session_t;
struct pn_link_t;

namespace proton {

/// **Experimental** - A work queue for serial execution.
///
/// Event handler functions associated with a single proton::connection are called in sequence.
/// The connection's @ref work_queue allows you to "inject" extra @ref work from any thread,
/// and have it executed in the same sequence.
///
/// You may also create arbitrary @ref work_queue objects backed by a @ref container that allow
/// other objects to have their own serialised work queues that can have work injected safely
/// from other threads. The @ref container ensures that the work is correctly serialised.
///
/// The @ref work class represents the work to be queued and can be created from a function
/// that takes no parameters and returns no value.
///

class work {
  public:
#if PN_CPP_HAS_STD_FUNCTION
    work(void_function0& f): item_( [&f]() { f(); }) {}
    template <class T>
    work(T f): item_(f) {}

    void operator()() { item_(); }
#else
    work(void_function0& f): item_(&f) {}

    void operator()() { (*item_)(); }
#endif
    ~work() {}


  private:
#if PN_CPP_HAS_STD_FUNCTION
    std::function<void()> item_;
#else
    void_function0* item_;
#endif
};

class PN_CPP_CLASS_EXTERN work_queue {
    /// @cond internal
    class impl;
    work_queue& operator=(impl* i);
    /// @endcond

  public:
    /// Create work_queue
    PN_CPP_EXTERN work_queue();
    PN_CPP_EXTERN work_queue(container&);

    PN_CPP_EXTERN ~work_queue();

#if PN_CPP_HAS_EXPLICIT_CONVERSIONS
    /// When using C++11 (or later) you can use work_queue in a bool context
    /// to indicate if there is an event loop set.
    PN_CPP_EXTERN explicit operator bool() const { return bool(impl_); }
#endif

    /// No event loop set.
    PN_CPP_EXTERN bool operator !() const { return !impl_; }

    /// Add work to the work queue: f() will be called serialised with other work in the queue:
    /// deferred and possibly in another thread.
    ///
    /// @return true if f() has or will be called, false if the event_loop is ended
    /// and f() cannot be injected.
    PN_CPP_EXTERN bool add(work f);

  private:
    PN_CPP_EXTERN static work_queue& get(pn_connection_t*);
    PN_CPP_EXTERN static work_queue& get(pn_session_t*);
    PN_CPP_EXTERN static work_queue& get(pn_link_t*);

    internal::pn_unique_ptr<impl> impl_;

    /// @cond INTERNAL
  friend class container;
  friend class io::connection_driver;
  template <class T> friend class thread_safe;
    /// @endcond
};

} // proton

#endif // PROTON_EVENT_LOOP_HPP
