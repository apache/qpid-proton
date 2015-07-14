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
#include "proton/messaging_event.hpp"
#include "proton/connection.hpp"
#include "proton/session.hpp"
#include "proton/messaging_adapter.hpp"
#include "proton/acceptor.hpp"
#include "proton/error.hpp"
#include "proton/url.hpp"

#include "msg.hpp"
#include "container_impl.hpp"
#include "connection_impl.hpp"
#include "connector.hpp"
#include "contexts.hpp"
#include "private_impl_ref.hpp"

#include "proton/connection.h"
#include "proton/session.h"
#include "proton/handlers.h"

namespace proton {

namespace {

connection_impl *impl(const connection &c) {
    return private_impl_ref<connection>::get(c);
}

} // namespace


class CHandler : public handler
{
  public:
    CHandler(pn_handler_t *h) : pn_handler_(h) {
        pn_incref(pn_handler_);
    }
    ~CHandler() {
        pn_decref(pn_handler_);
    }
    pn_handler_t *pn_handler() { return pn_handler_; }

    virtual void on_unhandled(event &e) {
        proton_event *pne = dynamic_cast<proton_event *>(&e);
        if (!pne) return;
        int type = pne->type();
        if (!type) return;  // Not from the reactor
        pn_handler_dispatch(pn_handler_, pne->pn_event(), (pn_event_type_t) type);
    }

  private:
    pn_handler_t *pn_handler_;
};


// Used to sniff for connector events before the reactor's global handler sees them.
class override_handler : public handler
{
  public:
    pn_handler_t *base_handler;

    override_handler(pn_handler_t *h) : base_handler(h) {
        pn_incref(base_handler);
    }
    ~override_handler() {
        pn_decref(base_handler);
    }


    virtual void on_unhandled(event &e) {
        proton_event *pne = dynamic_cast<proton_event *>(&e);
        // If not a Proton reactor event, nothing to override, nothing to pass along.
        if (!pne) return;
        int type = pne->type();
        if (!type) return;  // Also not from the reactor

        pn_event_t *cevent = pne->pn_event();
        pn_connection_t *conn = pn_event_connection(cevent);
        if (conn && type != PN_CONNECTION_INIT) {
            // send to override handler first
            connection_impl *connection = connection_context(conn);
            if (connection) {
                handler *override = connection->override();
                if (override) {
                    e.dispatch(*override);
                }
            }
        }

        pn_handler_dispatch(base_handler, cevent, (pn_event_type_t) type);

        if (conn && type == PN_CONNECTION_FINAL) {
            //  TODO:  this must be the last action of the last handler looking at
            //  connection events. Better: generate a custom FINAL event (or task).  Or move to
            //  separate event streams per connection as part of multi threading support.
            connection_impl *cimpl = connection_context(conn);
            if (cimpl)
                cimpl->reactor_detach();
            // TODO: remember all connections and do reactor_detach of zombie connections
            // not yet pn_connection_release'd at PN_REACTOR_FINAL.
        }
    }
};


namespace {

// TODO: configurable policy.  session_per_connection for now.
session default_session(pn_connection_t *conn, pn_session_t **ses) {
    if (!*ses) {
        *ses = pn_session(conn);
        pn_session_open(*ses);
    }
    return session(*ses);
}

struct inbound_context {
    static inbound_context* get(pn_handler_t* h) {
        return reinterpret_cast<inbound_context*>(pn_handler_mem(h));
    }
    container_impl *container_impl_;
    handler *cpp_handler_;
};

void cpp_handler_dispatch(pn_handler_t *c_handler, pn_event_t *cevent, pn_event_type_t type)
{
    // Ref counted per event, but when is the last event if stop() never called?
    container c(inbound_context::get(c_handler)->container_impl_);
    messaging_event mevent(cevent, type, c);
    mevent.dispatch(*inbound_context::get(c_handler)->cpp_handler_);
}

void cpp_handler_cleanup(pn_handler_t *c_handler)
{
}

pn_handler_t *cpp_handler(container_impl *c, handler *h)
{
    pn_handler_t *handler = pn_handler_new(cpp_handler_dispatch, sizeof(struct inbound_context), cpp_handler_cleanup);
    inbound_context *ctxt = inbound_context::get(handler);
    ctxt->container_impl_ = c;
    ctxt->cpp_handler_ = h;
    return handler;
}


} // namespace


void container_impl::incref(container_impl *impl_) {
    impl_->refcount_++;
}

void container_impl::decref(container_impl *impl_) {
    impl_->refcount_--;
    if (impl_->refcount_ == 0)
        delete impl_;
}

container_impl::container_impl(handler &h) :
    reactor_(0), handler_(&h), messaging_adapter_(0),
    override_handler_(0), flow_controller_(0), container_id_(),
    refcount_(0)
{}

container_impl::container_impl() :
    reactor_(0), handler_(0), messaging_adapter_(0),
    override_handler_(0), flow_controller_(0), container_id_(),
    refcount_(0)
{}

container_impl::~container_impl() {
    delete override_handler_;
    delete flow_controller_;
    delete messaging_adapter_;
    pn_reactor_free(reactor_);
}

connection container_impl::connect(const proton::url &url, handler *h) {
    if (!reactor_) throw error(MSG("container not started"));
    container ctnr(this);
    connection conn(ctnr, handler_);
    connector *ctor = new connector(conn);
    // connector self-deletes depending on reconnect logic
    ctor->address(url);  // TODO: url vector
    conn.override(ctor);
    conn.open();
    return conn;
}

pn_reactor_t *container_impl::reactor() { return reactor_; }


std::string container_impl::container_id() { return container_id_; }

duration container_impl::timeout() {
    pn_millis_t tmo = pn_reactor_get_timeout(reactor_);
    if (tmo == PN_MILLIS_MAX)
        return duration::FOREVER;
    return duration(tmo);
}

void container_impl::timeout(duration timeout) {
    if (timeout == duration::FOREVER || timeout.milliseconds > PN_MILLIS_MAX)
        pn_reactor_set_timeout(reactor_, PN_MILLIS_MAX);
    else {
        pn_millis_t tmo = timeout.milliseconds;
        pn_reactor_set_timeout(reactor_, tmo);
    }
}


sender container_impl::create_sender(connection &connection, const std::string &addr, handler *h) {
    if (!reactor_) throw error(MSG("container not started"));
    session session = default_session(connection.pn_connection(), &impl(connection)->default_session_);
    sender snd = session.create_sender(container_id_  + '-' + addr);
    pn_link_t *lnk = snd.pn_link();
    pn_terminus_set_address(pn_link_target(lnk), addr.c_str());
    if (h) {
        pn_record_t *record = pn_link_attachments(lnk);
        pn_record_set_handler(record, wrap_handler(h));
    }
    snd.open();
    return snd;
}

sender container_impl::create_sender(const proton::url &url) {
    if (!reactor_) throw error(MSG("container not started"));
    connection conn = connect(url, 0);
    session session = default_session(conn.pn_connection(), &impl(conn)->default_session_);
    std::string path = url.path();
    sender snd = session.create_sender(container_id_ + '-' + path);
    pn_terminus_set_address(pn_link_target(snd.pn_link()), path.c_str());
    snd.open();
    return snd;
}

receiver container_impl::create_receiver(connection &connection, const std::string &addr, handler *h) {
    if (!reactor_) throw error(MSG("container not started"));
    connection_impl *conn_impl = impl(connection);
    session session = default_session(conn_impl->pn_connection_, &conn_impl->default_session_);
    receiver rcv = session.create_receiver(container_id_ + '-' + addr);
    pn_link_t *lnk = rcv.pn_link();
    pn_terminus_set_address(pn_link_source(lnk), addr.c_str());
    if (h) {
        pn_record_t *record = pn_link_attachments(lnk);
        pn_record_set_handler(record, wrap_handler(h));
    }
    rcv.open();
    return rcv;
}

receiver container_impl::create_receiver(const proton::url &url) {
    if (!reactor_) throw error(MSG("container not started"));
    connection conn = connect(url, 0);
    session session = default_session(conn.pn_connection(), &impl(conn)->default_session_);
    std::string path = url.path();
    receiver rcv = session.create_receiver(container_id_ + '-' + path);
    pn_terminus_set_address(pn_link_source(rcv.pn_link()), path.c_str());
    rcv.open();
    return rcv;
}

class acceptor container_impl::acceptor(const proton::url& url) {
    pn_acceptor_t *acptr = pn_reactor_acceptor(reactor_, url.host().c_str(), url.port().c_str(), NULL);
    if (acptr)
        return proton::acceptor(acptr);
    else
        throw error(MSG("accept fail: " << pn_error_text(pn_io_error(pn_reactor_io(reactor_))) << "(" << url << ")"));
}

acceptor container_impl::listen(const proton::url &url) {
    if (!reactor_) throw error(MSG("container not started"));
    return acceptor(url);
}


pn_handler_t *container_impl::wrap_handler(handler *h) {
    return cpp_handler(this, h);
}


void container_impl::initialize_reactor() {
    if (reactor_) throw error(MSG("container already running"));
    reactor_ = pn_reactor();

    // Set our context on the reactor
    container_context(reactor_, this);

    if (handler_) {
        pn_handler_t *pn_handler = cpp_handler(this, handler_);
        pn_reactor_set_handler(reactor_, pn_handler);
        pn_decref(pn_handler);
    }

    // Set our own global handler that "subclasses" the existing one
    pn_handler_t *global_handler = pn_reactor_get_global_handler(reactor_);
    override_handler_ = new override_handler(global_handler);
    pn_handler_t *cpp_global_handler = cpp_handler(this, override_handler_);
    pn_reactor_set_global_handler(reactor_, cpp_global_handler);
    pn_decref(cpp_global_handler);

    // Note: we have just set up the following 4/5 handlers that see events in this order:
    // messaging_handler (Proton C events), pn_flowcontroller (optional), messaging_adapter,
    // messaging_handler (Messaging events from the messaging_adapter, i.e. the delegate),
    // connector override, the reactor's default globalhandler (pn_iohandler)
}

void container_impl::run() {
    initialize_reactor();
    pn_reactor_run(reactor_);
}

void container_impl::start() {
    initialize_reactor();
    pn_reactor_start(reactor_);
}

bool container_impl::process() {
    if (!reactor_) throw error(MSG("container not started"));
    bool result = pn_reactor_process(reactor_);
    // TODO: check errors
    return result;
}

void container_impl::stop() {
    if (!reactor_) throw error(MSG("container not started"));
    pn_reactor_stop(reactor_);
    // TODO: check errors
}

void container_impl::wakeup() {
    if (!reactor_) throw error(MSG("container not started"));
    pn_reactor_wakeup(reactor_);
    // TODO: check errors
}

bool container_impl::is_quiesced() {
    if (!reactor_) throw error(MSG("container not started"));
    return pn_reactor_quiesced(reactor_);
}

}
