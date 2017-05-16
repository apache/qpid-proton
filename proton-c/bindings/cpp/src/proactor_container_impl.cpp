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

#include "proactor_container_impl.hpp"
#include "proactor_event_loop_impl.hpp"

#include "proton/error_condition.hpp"
#include "proton/function.hpp"
#include "proton/listener.hpp"
#include "proton/listen_handler.hpp"
#include "proton/thread_safe.hpp"
#include "proton/url.hpp"

#include "proton/connection.h"
#include "proton/listener.h"
#include "proton/proactor.h"
#include "proton/transport.h"

#include "contexts.hpp"
#include "messaging_adapter.hpp"
#include "proton_bits.hpp"
#include "proton_event.hpp"

#include <assert.h>

#include <algorithm>
#include <vector>

namespace proton {

event_loop::impl::impl(pn_connection_t* c)
    : connection_(c), finished_(false)
{}

void event_loop::impl::finished() {
    finished_ = true;
}

#if PN_CPP_HAS_STD_FUNCTION
bool event_loop::impl::inject(std::function<void()> f) {
    // Note this is an unbounded work queue.
    // A resource-safe implementation should be bounded.
    if (finished_)
         return false;
    jobs_.push_back(f);
    pn_connection_wake(connection_);
    return true;
}

bool event_loop::impl::inject(proton::void_function0& f) {
    return inject([&f]() { f(); });
}

void event_loop::impl::run_all_jobs() {
    decltype(jobs_) j;
    {
        std::swap(j, jobs_);
    }
    // Run queued work, but ignore any exceptions
    for (auto& f : j) try {
        f();
    } catch (...) {};
}
#else
bool event_loop::impl::inject(proton::void_function0& f) {
    // Note this is an unbounded work queue.
    // A resource-safe implementation should be bounded.
    if (finished_)
         return false;
    jobs_.push_back(&f);
    pn_connection_wake(connection_);
    return true;
}

void event_loop::impl::run_all_jobs() {
    // Run queued work, but ignore any exceptions
    for (event_loop::impl::jobs::iterator f = jobs_.begin(); f != jobs_.end(); ++f) try {
        (**f)();
    } catch (...) {};
    jobs_.clear();
    return;
}
#endif
container::impl::impl(container& c, const std::string& id, messaging_handler* mh)
    : container_(c), proactor_(pn_proactor()), handler_(mh), id_(id),
      auto_stop_(true), stopping_(false)
{}

container::impl::~impl() {
    try {
        stop(error_condition("exception", "container shut-down"));
        //wait();
    } catch (...) {}
    pn_proactor_free(proactor_);
}

proton::connection container::impl::connect_common(
    const std::string& addr,
    const proton::connection_options& user_opts)
{
    if (stopping_)
        throw proton::error("container is stopping");

    connection_options opts = client_connection_options_; // Defaults
    opts.update(user_opts);
    messaging_handler* mh = opts.handler();

    proton::url url(addr);
    pn_connection_t *pnc = pn_connection();
    connection_context& cc(connection_context::get(pnc));
    cc.container = &container_;
    cc.handler = mh;
    cc.event_loop_ = new event_loop::impl(pnc);

    pn_connection_set_container(pnc, id_.c_str());
    pn_connection_set_hostname(pnc, url.host().c_str());
    if (!url.user().empty())
        pn_connection_set_user(pnc, url.user().c_str());
    if (!url.password().empty())
        pn_connection_set_password(pnc, url.password().c_str());

    connection conn = make_wrapper(pnc);
    conn.open(opts);
    // Figure out correct string len then create connection address
    int len = pn_proactor_addr(0, 0, url.host().c_str(), url.port().c_str());
    std::vector<char> caddr(len+1);
    pn_proactor_addr(&caddr[0], len+1, url.host().c_str(), url.port().c_str());
    pn_proactor_connect(proactor_, pnc, &caddr[0]);
    return conn;
}

proton::returned<proton::connection> container::impl::connect(
    const std::string& addr,
    const proton::connection_options& user_opts)
{
    connection conn = connect_common(addr, user_opts);
    return make_thread_safe(conn);
}

returned<sender> container::impl::open_sender(const std::string &url, const proton::sender_options &o1, const connection_options &o2) {
    proton::sender_options lopts(sender_options_);
    lopts.update(o1);
    connection conn = connect_common(url, o2);

    return make_thread_safe(conn.default_session().open_sender(proton::url(url).path(), lopts));
}

returned<receiver> container::impl::open_receiver(const std::string &url, const proton::receiver_options &o1, const connection_options &o2) {
    proton::receiver_options lopts(receiver_options_);
    lopts.update(o1);
    connection conn = connect_common(url, o2);

    return make_thread_safe(
        conn.default_session().open_receiver(proton::url(url).path(), lopts));
}

pn_listener_t* container::impl::listen_common_lh(const std::string& addr) {
    if (stopping_)
        throw proton::error("container is stopping");

    proton::url url(addr);

    // Figure out correct string len then create connection address
    int len = pn_proactor_addr(0, 0, url.host().c_str(), url.port().c_str());
    std::vector<char> caddr(len+1);
    pn_proactor_addr(&caddr[0], len+1, url.host().c_str(), url.port().c_str());

    pn_listener_t* listener = pn_listener();
    pn_proactor_listen(proactor_, listener, &caddr[0], 16);
    return listener;
}

proton::listener container::impl::listen(const std::string& addr) {
    pn_listener_t* listener = listen_common_lh(addr);
    return proton::listener(listener);
}

proton::listener container::impl::listen(const std::string& addr, const proton::connection_options& opts) {
    pn_listener_t* listener = listen_common_lh(addr);
    listener_context& lc=listener_context::get(listener);
    lc.connection_options_.reset(new connection_options(opts));
    return proton::listener(listener);
}

proton::listener container::impl::listen(const std::string& addr, proton::listen_handler& lh) {
    pn_listener_t* listener = listen_common_lh(addr);
    listener_context& lc=listener_context::get(listener);
    lc.listen_handler_ = &lh;
    return proton::listener(listener);
}

#if PN_CPP_HAS_STD_FUNCTION
void container::impl::schedule(duration delay, void_function0& f) {
    schedule(delay, [&f](){ f(); } );
}

void container::impl::schedule(duration delay, std::function<void()> f) {
    // Set timeout
    pn_proactor_set_timeout(proactor_, delay.milliseconds());

    // Record timeout; Add callback to timeout sorted list
    deferred_.emplace_back(scheduled{timestamp::now()+delay, f});
    std::push_heap(deferred_.begin(), deferred_.end());
}
#else
void container::impl::scheduled::task() {(*task_)();}

void container::impl::schedule(duration delay, void_function0& f) {
    // Set timeout
    pn_proactor_set_timeout(proactor_, delay.milliseconds());

    // Record timeout; Add callback to timeout sorted list
    scheduled s={timestamp::now()+delay, &f};
    deferred_.push_back(s);
    std::push_heap(deferred_.begin(), deferred_.end());
}
#endif

void container::impl::client_connection_options(const connection_options &opts) {
    client_connection_options_ = opts;
}

void container::impl::server_connection_options(const connection_options &opts) {
    server_connection_options_ = opts;
}

void container::impl::sender_options(const proton::sender_options &opts) {
    sender_options_ = opts;
}

void container::impl::receiver_options(const proton::receiver_options &opts) {
    receiver_options_ = opts;
}

void container::impl::run_timer_jobs() {
    // Check head of timer queue
    timestamp now = timestamp::now();
    scheduled* next = &deferred_.front();

    // So every scheduled element that has past run and remove head
    while ( next->time<=now ) {
        next->task();
        std::pop_heap(deferred_.begin(), deferred_.end());
        deferred_.pop_back();
        // If there are no more scheduled items finish now
        if  ( deferred_.size()==0 ) return;
        next = &deferred_.front();
    };

    // To get here we know we must have at least one more thing scheduled
    pn_proactor_set_timeout(proactor_, (next->time-now).milliseconds());
}

bool container::impl::handle(pn_event_t* event) {

    // If we have any pending connection work, do it now
    pn_connection_t* c = pn_event_connection(event);
    if (c) {
        event_loop::impl* loop = connection_context::get(c).event_loop_.impl_.get();
        loop->run_all_jobs();
    }

    // Process events that shouldn't be sent to messaging_handler
    switch (pn_event_type(event)) {

    case PN_PROACTOR_INACTIVE: /* listener and all connections closed */
        return auto_stop_;

    // We never interrupt the proactor so ignore
    case PN_PROACTOR_INTERRUPT:
        return false;

    case PN_PROACTOR_TIMEOUT:
        // Maybe we got a timeout and have nothing scheduled (not sure if this is possible)
        if  ( deferred_.size()==0 ) return false;

        run_timer_jobs();
        return false;

    case PN_LISTENER_OPEN:
        return false;

    case PN_LISTENER_ACCEPT: {
        pn_listener_t* l = pn_event_listener(event);
        pn_connection_t* c = pn_connection();
        listener_context &lc(listener_context::get(l));
        pn_connection_set_container(c, id_.c_str());
        connection_options opts = server_connection_options_;
        if (lc.listen_handler_) opts.update(lc.listen_handler_->on_accept());
        else if (!!lc.connection_options_) opts.update(*lc.connection_options_);
        lc.connection_options_.reset(new connection_options(opts));
        // Handler applied separately
        connection_context& cc = connection_context::get(c);
        cc.container = &container_;
        cc.listener_context_ = &lc;
        cc.handler = opts.handler();
        cc.event_loop_ = new event_loop::impl(c);
        pn_listener_accept(l, c);
        return false;
    }
    case PN_LISTENER_CLOSE: {
        pn_listener_t* l = pn_event_listener(event);
        listener_context &lc(listener_context::get(l));
        if (lc.listen_handler_) {
            pn_condition_t* c = pn_listener_condition(l);
            if (pn_condition_is_set(c)) {
                lc.listen_handler_->on_error(make_wrapper(c).what());
            }
            lc.listen_handler_->on_close();
        }
        return false;
    }
    // If the event was just connection wake then there isn't anything more to do
    case PN_CONNECTION_WAKE:
        return false;

    // Connection driver will bind a new transport to the connection at this point
    case PN_CONNECTION_INIT:
        return false;

    case PN_CONNECTION_BOUND: {
        // Need to apply post bind connection options
        pn_connection_t* c = pn_event_connection(event);
        connection conn = make_wrapper(c);
        connection_context& cc = connection_context::get(c);
        if (cc.listener_context_) {
            cc.listener_context_->connection_options_->apply_bound(conn);
        } else {
            client_connection_options_.apply_bound(conn);
        }

        return false;
    }
    default:
        break;
    }

    // Figure out the handler for the primary object for event
    messaging_handler* mh = 0;

    // First try for a link (send/receiver) handler
    pn_link_t *link = pn_event_link(event);
    if (link) mh = get_handler(link);

    // Try for session handler if no link handler
    pn_session_t *session = pn_event_session(event);
    if (session && !mh) mh = get_handler(session);

    // Try for connection handler if none of the above
    pn_connection_t *connection = pn_event_connection(event);
    if (connection && !mh) mh = get_handler(connection);

    // Use container handler if nothing more specific (must be a container handler)
    if (!mh) mh = handler_;

    // If we still have no handler don't do anything!
    // This is pretty unusual, but possible if we use the default constructor for container
    if (!mh) return false;

    // TODO: Currently create a throwaway messaging_adapter and proton_event so we can call dispatch, a bit inefficient
    messaging_adapter ma(*mh);
    proton_event pe(event, &container_);
    pe.dispatch(ma);
    return false;
}

void container::impl::thread(container::impl& ci) {
  bool finished = false;
  do {
    pn_event_batch_t *events = pn_proactor_wait(ci.proactor_);
    pn_event_t *e;
    while ((e = pn_event_batch_next(events))) {
      finished = ci.handle(e) || finished;
    }
    pn_proactor_done(ci.proactor_, events);
  } while(!finished);
}

void container::impl::run() {
    // Have to "manually" generate container events
    if (handler_) handler_->on_container_start(container_);
    thread(*this);
    if (handler_) handler_->on_container_stop(container_);
}

void container::impl::auto_stop(bool set) {
    auto_stop_ = set;
}

void container::impl::stop(const proton::error_condition& err) {
    auto_stop_ = true;
    stopping_ = true;
    pn_condition_t* error_condition = pn_condition();
    set_error_condition(err, error_condition);
    pn_proactor_disconnect(proactor_, error_condition);
    pn_condition_free(error_condition);
}

}
