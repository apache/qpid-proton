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

#include "reconnect_options_impl.hpp"

#include "proton/work_queue.hpp"
#include "proton/message.hpp"
#include "proton/internal/pn_unique_ptr.hpp"

struct pn_record_t;
struct pn_link_t;
struct pn_session_t;
struct pn_connection_t;
struct pn_listener_t;

namespace proton {

class proton_handler;
class connector;

namespace io {class link_namer;}

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

class listener_context;
class reconnect_context;

// Connection context used by all connections.
class connection_context : public context {
  public:
    connection_context();
    static connection_context& get(pn_connection_t *c);

    class container* container;
    pn_session_t *default_session; // Owned by connection.
    message event_message;      // re-used by messaging_adapter for performance.
    io::link_namer* link_gen;      // Link name generator.

    messaging_handler* handler;
    std::string reconnect_url_;
    std::vector<std::string> failover_urls_;
    internal::pn_unique_ptr<connection_options> connection_options_;
    internal::pn_unique_ptr<reconnect_context> reconnect_context_;
    listener_context* listener_context_;
    work_queue work_queue_;
};

class reconnect_options_base;

// This is not a context object on its own, but an optional part of connection
class reconnect_context {
  public:
    reconnect_context(const reconnect_options_base& ro);

    const reconnect_options_base reconnect_options_;
    duration delay_;
    int retries_;
    int current_url_;
    bool stop_reconnect_;
    bool reconnected_;
};

class listener_context : public context {
  public:
    listener_context();
    static listener_context& get(pn_listener_t* c);

    listen_handler* listen_handler_;
    internal::pn_unique_ptr<const connection_options> connection_options_;
};

class link_context : public context {
  public:
    link_context() : handler(0), credit_window(10), pending_credit(0), auto_accept(true), auto_settle(true), draining(false) {}
    static link_context& get(pn_link_t* l);

    messaging_handler* handler;
    int credit_window;
    uint32_t pending_credit;
    bool auto_accept;
    bool auto_settle;
    bool draining;
};

class session_context : public context {
  public:
    session_context() : handler(0) {}
    static session_context& get(pn_session_t* s);

    messaging_handler* handler;
};

}

#endif  /*!PROTON_CPP_CONTEXTS_H*/
