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

#include "proton/default_container.hpp"
#include "proton/connection_options.hpp"
#include "proton/connection.hpp"
#include "proton/session.hpp"
#include "proton/error.hpp"
#include "proton/event_loop.hpp"
#include "proton/sender.hpp"
#include "proton/receiver.hpp"
#include "proton/task.hpp"
#include "proton/ssl.hpp"
#include "proton/sasl.hpp"
#include "proton/transport.hpp"
#include "proton/url.hpp"
#include "proton/uuid.hpp"

#include "acceptor.hpp"
#include "connector.hpp"
#include "container_impl.hpp"
#include "contexts.hpp"
#include "messaging_adapter.hpp"
#include "msg.hpp"
#include "proton_bits.hpp"
#include "proton_event.hpp"

#include <proton/connection.h>
#include <proton/handlers.h>
#include <proton/reactor.h>
#include <proton/session.h>

namespace proton {

class handler_context {
  public:
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

    static void dispatch(pn_handler_t *c_handler, pn_event_t *c_event, pn_event_type_t)
    {
        handler_context& hc(handler_context::get(c_handler));
        proton_event pevent(c_event, hc.container_);
        pevent.dispatch(*hc.handler_);
        return;
    }

    container *container_;
    proton_handler *handler_;
};

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
            proton_handler *oh = connection_context::get(conn).handler.get();
            if (oh && type != proton_event::CONNECTION_INIT) {
                // Send event to connector
                pe.dispatch(*oh);
            }
            else if (!oh && type == proton_event::CONNECTION_INIT) {
                // Newly accepted connection from lister socket
                connection c(make_wrapper(conn));
                container_impl_.configure_server_connection(c);
            }
        }
        pn_handler_dispatch(base_handler.get(), cevent, pn_event_type_t(type));
    }
};

internal::pn_ptr<pn_handler_t> container_impl::cpp_handler(proton_handler *h) {
    pn_handler_t *handler = h ? pn_handler_new(&handler_context::dispatch,
                                               sizeof(class handler_context),
                                               &handler_context::cleanup) : 0;
    if (handler) {
        handler_context &hc = handler_context::get(handler);
        hc.container_ = this;
        hc.handler_ = h;
    }
    return internal::take_ownership(handler);
}

container_impl::container_impl(const std::string& id, messaging_handler *mh) :
    reactor_(reactor::create()),
    id_(id.empty() ? uuid::random().str() : id),
    auto_stop_(true)
{
    container_context::set(reactor_, *this);

    // Set our own global handler that "subclasses" the existing one
    pn_handler_t *global_handler = reactor_.pn_global_handler();
    proton_handler* oh = new override_handler(global_handler, *this);
    handlers_.push_back(oh);
    reactor_.pn_global_handler(cpp_handler(oh).get());
    if (mh) {
        proton_handler* h = new messaging_adapter(*mh);
        handlers_.push_back(h);
        reactor_.pn_handler(cpp_handler(h).get());
    }

    // Note: we have just set up the following handlers that see
    // events in this order: messaging_adapter, connector override,
    // the reactor's default globalhandler (pn_iohandler)
}

namespace {
void close_acceptor(acceptor a) {
    listen_handler*& lh = listener_context::get(unwrap(a)).listen_handler_;
    if (lh) {
        lh->on_close();
        lh = 0;
    }
    a.close();
}
}

container_impl::~container_impl() {
    for (acceptors::iterator i = acceptors_.begin(); i != acceptors_.end(); ++i)
        close_acceptor(i->second);
}

namespace {
// FIXME aconway 2016-06-07: this is not thread safe. It is sufficient for using
// default_container::schedule() inside a handler but not for inject() from
// another thread.
struct immediate_event_loop : public event_loop {
    virtual bool inject(void_function0& f) PN_CPP_OVERRIDE {
        try { f(); } catch(...) {}
        return true;
    }

#if PN_CPP_HAS_CPP11
    virtual bool inject(std::function<void()> f) PN_CPP_OVERRIDE {
        try { f(); } catch(...) {}
        return true;
    }
#endif
};
}

returned<connection> container_impl::connect(const std::string &urlstr, const connection_options &user_opts) {
    connection_options opts = client_connection_options(); // Defaults
    opts.update(user_opts);
    messaging_handler* mh = opts.handler();
    internal::pn_ptr<pn_handler_t> chandler;
    if (mh) {
        proton_handler* h = new messaging_adapter(*mh);
        handlers_.push_back(h);
        chandler = cpp_handler(h);
    }

    proton::url  url(urlstr);
    connection conn(reactor_.connection_to_host(url.host(), url.port(), chandler.get()));
    internal::pn_unique_ptr<connector> ctor(new connector(conn, opts, url));
    connection_context& cc(connection_context::get(conn));
    cc.handler.reset(ctor.release());
    cc.event_loop.reset(new immediate_event_loop);

    pn_connection_t *pnc = unwrap(conn);
    pn_connection_set_container(pnc, id_.c_str());
    pn_connection_set_hostname(pnc, url.host().c_str());
    if (!url.user().empty())
        pn_connection_set_user(pnc, url.user().c_str());
    if (!url.password().empty())
        pn_connection_set_password(pnc, url.password().c_str());

    conn.open(opts);
    return make_thread_safe(conn);
}

returned<sender> container_impl::open_sender(const std::string &url, const proton::sender_options &o1, const connection_options &o2) {
    proton::sender_options lopts(sender_options_);
    lopts.update(o1);
    connection_options copts(client_connection_options_);
    copts.update(o2);
    connection conn = connect(url, copts);
    return make_thread_safe(conn.default_session().open_sender(proton::url(url).path(), lopts));
}

returned<receiver> container_impl::open_receiver(const std::string &url, const proton::receiver_options &o1, const connection_options &o2) {
    proton::receiver_options lopts(receiver_options_);
    lopts.update(o1);
    connection_options copts(client_connection_options_);
    copts.update(o2);
    connection conn = connect(url, copts);
    return make_thread_safe(
        conn.default_session().open_receiver(proton::url(url).path(), lopts));
}

listener container_impl::listen(const std::string& url, listen_handler& lh) {
    if (acceptors_.find(url) != acceptors_.end())
        throw error("already listening on " + url);
    connection_options opts = server_connection_options(); // Defaults

    messaging_handler* mh = opts.handler();
    internal::pn_ptr<pn_handler_t> chandler;
    if (mh) {
        proton_handler* h = new messaging_adapter(*mh);
        handlers_.push_back(h);
        chandler = cpp_handler(h);
    }

    proton::url u(url);
    pn_acceptor_t *acptr = pn_reactor_acceptor(
        reactor_.pn_object(), u.host().c_str(), u.port().c_str(), chandler.get());
    if (!acptr) {
        std::string err(pn_error_text(pn_reactor_error(reactor_.pn_object())));
        lh.on_error(err);
        lh.on_close();
        throw error(err);
    }
    // Do not use pn_acceptor_set_ssl_domain().  Manage the incoming connections ourselves for
    // more flexibility (i.e. ability to change the server cert for a long running listener).
    listener_context& lc(listener_context::get(acptr));
    lc.listen_handler_ = &lh;
    lc.ssl = u.scheme() == url::AMQPS;
    listener_context::get(acptr).listen_handler_ = &lh;
    acceptors_[url] = make_wrapper(acptr);
    return listener(*this, url);
}

void container_impl::stop_listening(const std::string& url) {
    acceptors::iterator i = acceptors_.find(url);
    if (i != acceptors_.end())
        close_acceptor(i->second);
}

task container_impl::schedule(container& c, int delay, proton_handler *h) {
    container_impl& ci = static_cast<container_impl&>(c);
    internal::pn_ptr<pn_handler_t> task_handler;
    if (h)
        task_handler = ci.cpp_handler(h);
    return ci.reactor_.schedule(delay, task_handler.get());
}

namespace {
// Abstract base for timer_handler_std and timer_handler_03
struct timer_handler : public proton_handler, public void_function0 {
    void on_timer_task(proton_event& ) PN_CPP_OVERRIDE {
        (*this)();
        delete this;
    }
    void on_reactor_final(proton_event&) PN_CPP_OVERRIDE {
        delete this;
    }
};

struct timer_handler_03 : public timer_handler {
    void_function0& func;
    timer_handler_03(void_function0& f): func(f) {}
    void operator()() PN_CPP_OVERRIDE { func(); }
};
}

void container_impl::schedule(duration delay, void_function0& f) {
    schedule(*this, delay.milliseconds(), new timer_handler_03(f));
}

#if PN_CPP_HAS_STD_FUNCTION
namespace {
struct timer_handler_std : public timer_handler {
    std::function<void()> func;
    timer_handler_std(std::function<void()> f): func(f) {}
    void operator()() PN_CPP_OVERRIDE { func(); }
};
}

void container_impl::schedule(duration delay, std::function<void()> f) {
    schedule(*this, delay.milliseconds(), new timer_handler_std(f));
}
#endif

void container_impl::client_connection_options(const connection_options &opts) {
    client_connection_options_ = opts;
}

void container_impl::server_connection_options(const connection_options &opts) {
    server_connection_options_ = opts;
}

void container_impl::sender_options(const proton::sender_options &opts) {
    sender_options_ = opts;
}

void container_impl::receiver_options(const proton::receiver_options &opts) {
    receiver_options_ = opts;
}

void container_impl::configure_server_connection(connection &c) {
    pn_acceptor_t *pnp = pn_connection_acceptor(unwrap(c));
    listener_context &lc(listener_context::get(pnp));
    pn_connection_set_container(unwrap(c), id_.c_str());
    connection_options opts = server_connection_options_;
    opts.update(lc.get_options());
    // Unbound options don't apply to server connection
    opts.apply_bound(c);
    // Handler applied separately
    messaging_handler* mh = opts.handler();
    if (mh) {
        proton_handler* h = new messaging_adapter(*mh);
        handlers_.push_back(h);
        internal::pn_ptr<pn_handler_t> chandler = cpp_handler(h);
        pn_record_t *record = pn_connection_attachments(unwrap(c));
        pn_record_set_handler(record, chandler.get());
    }
    connection_context::get(c).event_loop.reset(new immediate_event_loop);
}

void container_impl::run() {
    do {
        reactor_.run();
    } while (!auto_stop_);
}

void container_impl::stop(const error_condition&) {
    reactor_.stop();
    auto_stop_ = true;
}

void container_impl::auto_stop(bool set) {
    auto_stop_ = set;
}

#if PN_CPP_HAS_UNIQUE_PTR
std::unique_ptr<container> make_default_container(messaging_handler& h, const std::string& id) {
    return std::unique_ptr<container>(new container_impl(id, &h));
}
std::unique_ptr<container> make_default_container(const std::string& id) {
  return std::unique_ptr<container>(new container_impl(id));
}
#endif

// Avoid deprecated diagnostics from auto_ptr
#if defined(__GNUC__) && __GNUC__*100 + __GNUC_MINOR__ >= 406 || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

std::auto_ptr<container> make_auto_default_container(messaging_handler& h, const std::string& id) {
  return std::auto_ptr<container>(new container_impl(id, &h));
}
std::auto_ptr<container> make_auto_default_container(const std::string& id) {
  return std::auto_ptr<container>(new container_impl(id));
}

#if defined(__GNUC__) && __GNUC__*100 + __GNUC_MINOR__ >= 406 || defined(__clang__)
#pragma GCC diagnostic pop
#endif
}
