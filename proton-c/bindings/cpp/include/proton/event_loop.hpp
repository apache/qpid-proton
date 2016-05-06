#ifndef PROTON_IO_EVENT_LOOP_HPP
#define PROTON_IO_EVENT_LOOP_HPP

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

#include <proton/config.hpp>

#include <functional>

#if PN_CPP_HAS_CPP11
#include <future>
#include <type_traits>
#endif

struct pn_connection_t;
struct pn_session_t;
struct pn_link_t;

namespace proton {

// FIXME aconway 2016-05-04: doc

class inject_handler {
  public:
    virtual ~inject_handler() {}
    virtual void on_inject() = 0;
};

class PN_CPP_CLASS_EXTERN event_loop {
 public:
    virtual ~event_loop() {}

    // FIXME aconway 2016-05-05: doc, note bool return not throw because no
    // atomic way to determine closd status and throw during shutdown is bad.
    virtual bool inject(inject_handler&) = 0;
#if PN_CPP_HAS_CPP11
    virtual bool inject(std::function<void()>) = 0;
#endif

 protected:
    event_loop() {}

 private:
    PN_CPP_EXTERN static event_loop* get(pn_connection_t*);
    PN_CPP_EXTERN static event_loop* get(pn_session_t*);
    PN_CPP_EXTERN static event_loop* get(pn_link_t*);

  friend class connection;
 template <class T> friend class thread_safe;
};

}

#endif // PROTON_IO_EVENT_LOOP_HPP
