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
#include "proton/connection_engine.hpp"

#include "id_generator.hpp"
#include "proton_handler.hpp"

struct pn_session_t;
struct pn_event_t;
struct pn_reactor_t;
struct pn_record_t;
struct pn_acceptor_t;

namespace proton {

class proton_handler;

// Base class for C++ classes that are used as proton contexts.
// Contexts are pn_objects managed by pn reference counts, the C++ value is allocated in-place.
class context {
  public:
    // identifies a context, contains a record pointer and a handle.
    typedef std::pair<pn_record_t*, pn_handle_t> id;

    virtual ~context();

    // Allocate a default-constructed T as a proton object.
    // T must be a subclass of context.
    template <class T> static T *create() { return new(alloc(sizeof(T))) T(); }

    // The pn_class for a context
    static pn_class_t* pn_class();

    // Get the context identified by id as a C++ T*, return null pointer if not present.
    template <class T> static T* ptr(id id_) {
        return reinterpret_cast<T*>(pn_record_get(id_.first, id_.second));
    }

    // If the context is not present, create it with value x.
    template <class T> static T& ref(id id_) {
        T* ctx = context::ptr<T>(id_);
        if (!ctx) {
            ctx = create<T>();
            pn_record_def(id_.first, id_.second, pn_class());
            pn_record_set(id_.first, id_.second, ctx);
            pn_decref(ctx);
        }
        return *ctx;
    }

  private:
    static void *alloc(size_t n);
};

// Connection context used by all connections.
class connection_context : public context {
  public:
    connection_context() : default_session(0) {}

    // Used by all connections
    pn_session_t *default_session; // Owned by connection.
    message event_message;      // re-used by messaging_adapter for performance.
    id_generator link_gen;      // Link name generator.

    internal::pn_unique_ptr<proton_handler> handler;

    static connection_context& get(pn_connection_t *c) { return ref<connection_context>(id(c)); }
    static connection_context& get(const connection& c) { return ref<connection_context>(id(c)); }

  protected:
    static context::id id(pn_connection_t*);
    static context::id id(const connection& c) { return id(c.pn_object()); }
};

// Connection context with information used by the connection_engine.
class connection_engine_context : public connection_context {
  public:
    connection_engine_context() :  engine_handler(0), transport(0), collector(0) {}

    class handler *engine_handler;
    pn_transport_t  *transport;
    pn_collector_t  *collector;
    static connection_engine_context& get(const connection &c) {
        return ref<connection_engine_context>(id(c));
    }
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
