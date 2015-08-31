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
#include "proton/event.hpp"
#include "proton/messaging_event.hpp"
#include "proton/connection.hpp"
#include "proton/session.hpp"
#include "proton/messaging_adapter.hpp"
#include "proton/acceptor.hpp"
#include "proton/error.hpp"
#include "proton/url.hpp"
#include "proton/sender.hpp"
#include "proton/receiver.hpp"

#include "msg.hpp"
#include "container_impl.hpp"
#include "connector.hpp"
#include "contexts.hpp"
#include "private_impl_ref.hpp"

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

    static void dispatch(pn_handler_t *c_handler, pn_event_t *c_event, pn_event_type_t type)
    {
        handler_context& hc(handler_context::get(c_handler));
        messaging_event mevent(c_event, type, *hc.container_);
        mevent.dispatch(*hc.handler_);
        return;
    }

    container *container_;
    handler *handler_;
};


// Used to sniff for connector events before the reactor's global handler sees them.
class override_handler : public handler
{
  public:
    counted_ptr<pn_handler_t> base_handler;

    override_handler(pn_handler_t *h) : base_handler(h) {}

    virtual void on_unhandled(event &e) {
        proton_event *pne = dynamic_cast<proton_event *>(&e);
        // If not a Proton reactor event, nothing to override, nothing to pass along.
        if (!pne) return;
        int type = pne->type();
        if (!type) return;  // Also not from the reactor

        pn_event_t *cevent = pne->pn_event();
        pn_connection_t *conn = pn_event_connection(cevent);
        if (conn && type != PN_CONNECTION_INIT) {
            handler *override = connection_context::get(conn).handler;
            if (override) e.dispatch(*override);
        }
        pn_handler_dispatch(base_handler.get(), cevent, (pn_event_type_t) type);
    }
};

} // namespace

counted_ptr<pn_handler_t> container_impl::cpp_handler(handler *h)
{
    counted_ptr<pn_handler_t> handler(
        pn_handler_new(&handler_context::dispatch, sizeof(struct handler_context),
                       &handler_context::cleanup),
        false);
    handler_context &hc = handler_context::get(handler.get());
    hc.container_ = &container_;
    hc.handler_ = h;
    return handler;
}

container_impl::container_impl(container& c, handler *h) :
    container_(c), reactor_(reactor::create()), handler_(h)
{
    container_context(pn_cast(reactor_.get()), container_);

    // Set our own global handler that "subclasses" the existing one
    pn_handler_t *global_handler = pn_reactor_get_global_handler(pn_cast(reactor_.get()));
    override_handler_.reset(new override_handler(global_handler));
    counted_ptr<pn_handler_t> cpp_global_handler(cpp_handler(override_handler_.get()));
    pn_reactor_set_global_handler(pn_cast(reactor_.get()), cpp_global_handler.get());
    if (handler_) {
        counted_ptr<pn_handler_t> pn_handler(cpp_handler(handler_));
        pn_reactor_set_handler(pn_cast(reactor_.get()), pn_handler.get());
    }


    // Note: we have just set up the following handlers that see events in this order:
    // messaging_handler (Proton C events), pn_flowcontroller (optional), messaging_adapter,
    // messaging_handler (Messaging events from the messaging_adapter, i.e. the delegate),
    // connector override, the reactor's default globalhandler (pn_iohandler)
}

container_impl::~container_impl() {}

connection& container_impl::connect(const proton::url &url, handler *h) {
    counted_ptr<pn_handler_t> chandler = h ? cpp_handler(h) : counted_ptr<pn_handler_t>();
    connection* conn =
        connection::cast(pn_reactor_connection(pn_cast(reactor_.get()), chandler.get()));
    connector *ctor = new connector(*conn); // Will be deleted by connection_context
    ctor->address(url);  // TODO: url vector
    connection_context& cc(connection_context::get(pn_cast(conn)));
    cc.container_impl = this;
    cc.handler = ctor;
    conn->open();
    return *conn;
}

sender& container_impl::create_sender(connection &connection, const std::string &addr, handler *h) {
    sender& snd = connection.default_session().create_sender(container_id_  + '-' + addr);
    snd.target().address(addr);
    if (h) snd.handler(*h);
    snd.open();
    return snd;
}

sender& container_impl::create_sender(const proton::url &url) {
    connection& conn = connect(url, 0);
    std::string path = url.path();
    sender& snd = conn.default_session().create_sender(container_id_ + '-' + path);
    snd.target().address(path);
    snd.open();
    return snd;
}

receiver& container_impl::create_receiver(connection &conn, const std::string &addr, bool dynamic, handler *h)
{
    receiver& rcv = conn.default_session().create_receiver(container_id_ + '-' + addr);
    rcv.source().address(addr);
    if (dynamic) rcv.source().dynamic(true);
    if (h) rcv.handler(*h);
    rcv.open();
    return rcv;
}

receiver& container_impl::create_receiver(const proton::url &url) {
    connection& conn = connect(url, 0);
    std::string path = url.path();
    receiver& rcv = conn.default_session().create_receiver(container_id_ + '-' + path);
    pn_terminus_set_address(pn_link_source(pn_cast(&rcv)), path.c_str());
    rcv.open();
    return rcv;
}

acceptor& container_impl::listen(const proton::url& url) {
    pn_acceptor_t *acptr = pn_reactor_acceptor(
        pn_cast(reactor_.get()), url.host().c_str(), url.port().c_str(), NULL);
    if (acptr)
        return *acceptor::cast(acptr);
    else
        throw error(MSG("accept fail: " <<
                        pn_error_text(pn_io_error(pn_reactor_io(pn_cast(reactor_.get()))))
                        << "(" << url << ")"));
}

}
