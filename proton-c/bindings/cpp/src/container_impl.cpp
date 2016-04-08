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
#include "proton/container.hpp"
#include "proton/connection_options.hpp"
#include "proton/connection.hpp"
#include "proton/session.hpp"
#include "proton/acceptor.hpp"
#include "proton/error.hpp"
#include "proton/url.hpp"
#include "proton/sender.hpp"
#include "proton/receiver.hpp"
#include "proton/task.hpp"
#include "proton/ssl.hpp"
#include "proton/sasl.hpp"
#include "proton/transport.hpp"
#include "proton/uuid.hpp"

#include "connector.hpp"
#include "container_impl.hpp"
#include "contexts.hpp"
#include "messaging_adapter.hpp"
#include "msg.hpp"
#include "proton_event.hpp"

#include "proton/connection.h"
#include "proton/session.h"
#include "proton/handlers.h"
#include "proton/reactor.h"

namespace proton {

namespace {

struct handler_context {
    static handler_context& get(pn_handler_t* h) {
        return *reinterpret_cast<handler_context*>(pn_handler_mem(h));
    }
    static void cleanup(pn_handler_t*) {}

    /*
     * NOTE: this call, at the transition from C to C++ is possibly
     * the biggest performance bottleneck.  "Average" clients ignore
     * 90% of these events.  Current strategy is to create the
     * messaging_event on the stack.  For success, the messaging_event
     * should be small and free of indirect malloc/free/new/delete.
     */

    static void dispatch(pn_handler_t *c_handler, pn_event_t *c_event, pn_event_type_t type)
    {
        handler_context& hc(handler_context::get(c_handler));
        proton_event pevent(c_event, type, hc.container_);
        pevent.dispatch(*hc.handler_);
        return;
    }

    container *container_;
    proton_handler *handler_;
};

} // namespace

// Used to sniff for connector events before the reactor's global handler sees them.
class override_handler : public proton_handler
{
  public:
    internal::pn_ptr<pn_handler_t> base_handler;
    container_impl &container_impl_;

    override_handler(pn_handler_t *h, container_impl &c) : base_handler(h), container_impl_(c) {}

    virtual void on_unhandled(proton_event &pe) {
        proton_event::event_type type = pe.type();
        if (type==proton_event::EVENT_NONE) return;  // Also not from the reactor

        pn_event_t *cevent = pe.pn_event();
        pn_connection_t *conn = pn_event_connection(cevent);
        if (conn) {
            proton_handler *override = connection_context::get(conn).handler.get();
            if (override && type != proton_event::CONNECTION_INIT) {
                // Send event to connector
                pe.dispatch(*override);
            }
            else if (!override && type == proton_event::CONNECTION_INIT) {
                // Newly accepted connection from lister socket
                connection c(conn);
                container_impl_.configure_server_connection(c);
            }
        }
        pn_handler_dispatch(base_handler.get(), cevent, pn_event_type_t(type));
    }
};

internal::pn_ptr<pn_handler_t> container_impl::cpp_handler(proton_handler *h) {
    pn_handler_t *handler = pn_handler_new(&handler_context::dispatch,
                                           sizeof(struct handler_context),
                                           &handler_context::cleanup);
    handler_context &hc = handler_context::get(handler);
    hc.container_ = &container_;
    hc.handler_ = h;
    return internal::take_ownership(handler);
}

container_impl::container_impl(container& c, messaging_adapter *h, const std::string& id) :
    container_(c), reactor_(reactor::create()), handler_(h),
    id_(id.empty() ? uuid::random().str() : id), id_gen_()
{
    container_context::set(reactor_, container_);

    // Set our own global handler that "subclasses" the existing one
    pn_handler_t *global_handler = reactor_.pn_global_handler();
    override_handler_.reset(new override_handler(global_handler, *this));
    internal::pn_ptr<pn_handler_t> cpp_global_handler(cpp_handler(override_handler_.get()));
    reactor_.pn_global_handler(cpp_global_handler.get());
    if (handler_) {
        reactor_.pn_handler(cpp_handler(handler_).get());
    }

    // Note: we have just set up the following handlers that see
    // events in this order: messaging_adapter, connector override,
    // the reactor's default globalhandler (pn_iohandler)
}

container_impl::~container_impl() {}

connection container_impl::connect(const proton::url &url, const connection_options &user_opts) {
    connection_options opts = client_connection_options(); // Defaults
    opts.update(user_opts);
    proton_handler *h = opts.handler();

    internal::pn_ptr<pn_handler_t> chandler = h ? cpp_handler(h) : internal::pn_ptr<pn_handler_t>();
    connection conn(reactor_.connection(chandler.get()));
    internal::pn_unique_ptr<connector> ctor(new connector(conn, opts));
    ctor->address(url);  // TODO: url vector
    connection_context& cc(connection_context::get(conn));
    cc.handler.reset(ctor.release());
    cc.link_gen.prefix(id_gen_.next() + "/");
    pn_connection_set_container(conn.pn_object(), id_.c_str());

    conn.open();
    return conn;
}

sender container_impl::open_sender(const proton::url &url, const proton::link_options &o1, const connection_options &o2) {
    proton::link_options lopts(link_options_);
    lopts.update(o1);
    connection_options copts(client_connection_options_);
    copts.update(o2);
    connection conn = connect(url, copts);
    std::string path = url.path();
    return conn.default_session().open_sender(path, lopts);
}

receiver container_impl::open_receiver(const proton::url &url, const proton::link_options &o1, const connection_options &o2) {
    proton::link_options lopts(link_options_);
    lopts.update(o1);
    connection_options copts(client_connection_options_);
    copts.update(o2);
    connection conn = connect(url, copts);
    std::string path = url.path();
    return conn.default_session().open_receiver(path, lopts);
}

acceptor container_impl::listen(const proton::url& url, const connection_options &user_opts) {
    connection_options opts = server_connection_options(); // Defaults
    opts.update(user_opts);
    proton_handler *h = opts.handler();
    internal::pn_ptr<pn_handler_t> chandler = h ? cpp_handler(h) : internal::pn_ptr<pn_handler_t>();
    pn_acceptor_t *acptr = pn_reactor_acceptor(reactor_.pn_object(), url.host().c_str(), url.port().c_str(), chandler.get());
    if (!acptr)
        throw error(MSG("accept fail: " <<
                        pn_error_text(pn_io_error(reactor_.pn_io())))
                        << "(" << url << ")");
    // Do not use pn_acceptor_set_ssl_domain().  Manage the incoming connections ourselves for
    // more flexibility (i.e. ability to change the server cert for a long running listener).
    listener_context& lc(listener_context::get(acptr));
    lc.connection_options = opts;
    lc.ssl = url.scheme() == url::AMQPS;
    return acceptor(acptr);
}

task container_impl::schedule(int delay, proton_handler *h) {
    internal::pn_ptr<pn_handler_t> task_handler;
    if (h)
        task_handler = cpp_handler(h);
    return reactor_.schedule(delay, task_handler.get());
}

void container_impl::client_connection_options(const connection_options &opts) {
    client_connection_options_ = opts;
}

void container_impl::server_connection_options(const connection_options &opts) {
    server_connection_options_ = opts;
}

void container_impl::link_options(const proton::link_options &opts) {
    link_options_ = opts;
}

void container_impl::configure_server_connection(connection &c) {
    pn_acceptor_t *pnp = pn_connection_acceptor(connection_options::pn_connection(c));
    listener_context &lc(listener_context::get(pnp));
    connection_context::get(c).link_gen.prefix(id_gen_.next() + "/");
    pn_connection_set_container(c.pn_object(), id_.c_str());
    lc.connection_options.apply(c);
}

}
