#ifndef PROTON_CPP_CONTEXTS_H
#define PROTON_CPP_CONTEXTS_H

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

#include "proton/pn_unique_ptr.hpp"
#include "proton/message.hpp"
#include "proton/connection.hpp"
#include "proton/container.hpp"

#include "proton_handler.hpp"

struct pn_session_t;
struct pn_event_t;
struct pn_record_t;
struct pn_acceptor_t;

namespace proton {

class proton_handler;
class container_impl;

// Base class for C++ classes that are used as proton contexts.
// contexts are pn_objects managed by pn reference counts.
class context {
  public:
    virtual ~context();

    // Allocate a default-constructed T as a proton object. T must be a subclass of context.
    template <class T> static T *create() { return new(alloc(sizeof(T))) T(); }

    // Allocate a copy-constructed T as a proton object. T must be a subclass of context.
    template <class T> static T *create(const T& x) { return new(alloc(sizeof(T))) T(x); }

    static pn_class_t* pn_class();

  private:
    static void *alloc(size_t n);
};

class connection_context : public context {
  public:
    static connection_context& get(pn_connection_t*);
    static connection_context& get(const connection&);

    connection_context() : default_session(0), container_impl(0) {}

    pn_unique_ptr<proton_handler> handler;
    pn_session_t *default_session;   // Owned by connection
    class container_impl* container_impl;
    message event_message;  // re-used by messaging_adapter for performance
};

void container_context(const reactor&, container&);

class container_context {
  public:
    static void set(const reactor& r, container& c);
    static container& get(pn_reactor_t*);
};

class listener_context : public context {
  public:
    static listener_context& get(pn_acceptor_t* c);
    listener_context() : ssl(false) {}
    class connection_options connection_options;
    bool ssl;
};

}

#endif  /*!PROTON_CPP_CONTEXTS_H*/
