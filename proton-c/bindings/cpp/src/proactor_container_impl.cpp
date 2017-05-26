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
#include "proactor_work_queue_impl.hpp"

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

#include <assert.h>

#include <algorithm>
#include <vector>

#if PN_CPP_SUPPORTS_THREADS
# include <thread>
#endif

namespace proton {

class container::impl::common_work_queue : public work_queue::impl {
  public:
    common_work_queue(container::impl& c): container_(c), finished_(false), running_(false) {}

    typedef std::vector<work> jobs;

    void run_all_jobs();
    void finished() { GUARD(lock_); finished_ = true; }
    void schedule(duration, work);

    MUTEX(lock_)
    container::impl& container_;
    jobs jobs_;
    bool finished_;
    bool running_;
};

void container::impl::common_work_queue::schedule(duration d, work f) {
    // Note this is an unbounded work queue.
    // A resource-safe implementation should be bounded.
    if (finished_) return;
    container_.schedule(d, make_work(&work_queue::impl::add, (work_queue::impl*)this, f));
}

void container::impl::common_work_queue::run_all_jobs() {
    jobs j;
    // Lock this operation for mt
    {
        GUARD(lock_);
        // Ensure that we never run work from this queue concurrently
        if (running_) return;
        running_ = true;
        // But allow adding to the queue concurrently to running
        std::swap(j, jobs_);
    }
    // Run queued work, but ignore any exceptions
    for (jobs::iterator f = j.begin(); f != j.end(); ++f) try {
        (*f)();
    } catch (...) {};
    {
        GUARD(lock_);
        running_ = false;
    }
    return;
}

class container::impl::connection_work_queue : public common_work_queue {
  public:
    connection_work_queue(container::impl& ct, pn_connection_t* c): common_work_queue(ct), connection_(c) {}

    bool add(work f);

    pn_connection_t* connection_;
};

bool container::impl::connection_work_queue::add(work f) {
    // Note this is an unbounded work queue.
    // A resource-safe implementation should be bounded.
    GUARD(lock_);
    if (finished_) return false;
    jobs_.push_back(f);
    pn_connection_wake(connection_);
    return true;
}

class container::impl::container_work_queue : public common_work_queue {
  public:
    container_work_queue(container::impl& c): common_work_queue(c) {}
    ~container_work_queue() { container_.remove_work_queue(this); }

    bool add(work f);
};

bool container::impl::container_work_queue::add(work f) {
    // Note this is an unbounded work queue.
    // A resource-safe implementation should be bounded.
    GUARD(lock_);
    if (finished_) return false;
    jobs_.push_back(f);
    pn_proactor_set_timeout(container_.proactor_, 0);
    return true;
}

class work_queue::impl* container::impl::make_work_queue(container& c) {
    return c.impl_->add_work_queue();
}

container::impl::impl(container& c, const std::string& id, messaging_handler* mh)
    : threads_(0), container_(c), proactor_(pn_proactor()), handler_(mh), id_(id),
      auto_stop_(true), stopping_(false)
{}

container::impl::~impl() {
    pn_proactor_free(proactor_);
}

container::impl::container_work_queue* container::impl::add_work_queue() {
    container_work_queue* c = new container_work_queue(*this);
    work_queues_.insert(c);
    return c;
}

void container::impl::remove_work_queue(container::impl::container_work_queue* l) {
    work_queues_.erase(l);
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
    cc.work_queue_ = new container::impl::connection_work_queue(*container_.impl_, pnc);

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
    GUARD(lock_);
    return make_thread_safe(conn);
}

returned<sender> container::impl::open_sender(const std::string &url, const proton::sender_options &o1, const connection_options &o2) {
    proton::sender_options lopts(sender_options_);
    lopts.update(o1);
    connection conn = connect_common(url, o2);

    GUARD(lock_);
    return make_thread_safe(conn.default_session().open_sender(proton::url(url).path(), lopts));
}

returned<receiver> container::impl::open_receiver(const std::string &url, const proton::receiver_options &o1, const connection_options &o2) {
    proton::receiver_options lopts(receiver_options_);
    lopts.update(o1);
    connection conn = connect_common(url, o2);

    GUARD(lock_);
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
    GUARD(lock_);
    pn_listener_t* listener = listen_common_lh(addr);
    return proton::listener(listener);
}

proton::listener container::impl::listen(const std::string& addr, const proton::connection_options& opts) {
    GUARD(lock_);
    pn_listener_t* listener = listen_common_lh(addr);
    listener_context& lc=listener_context::get(listener);
    lc.connection_options_.reset(new connection_options(opts));
    return proton::listener(listener);
}

proton::listener container::impl::listen(const std::string& addr, proton::listen_handler& lh) {
    GUARD(lock_);
    pn_listener_t* listener = listen_common_lh(addr);
    listener_context& lc=listener_context::get(listener);
    lc.listen_handler_ = &lh;
    return proton::listener(listener);
}

void container::impl::schedule(duration delay, work f) {
    GUARD(lock_);
    timestamp now = timestamp::now();

    // Record timeout; Add callback to timeout sorted list
    scheduled s = {now+delay, f};
    deferred_.push_back(s);
    std::push_heap(deferred_.begin(), deferred_.end());

    // Set timeout for current head of timeout queue
    scheduled* next = &deferred_.front();
    pn_proactor_set_timeout(proactor_, (next->time-now).milliseconds());
}

void container::impl::client_connection_options(const connection_options &opts) {
    GUARD(lock_);
    client_connection_options_ = opts;
}

void container::impl::server_connection_options(const connection_options &opts) {
    GUARD(lock_);
    server_connection_options_ = opts;
}

void container::impl::sender_options(const proton::sender_options &opts) {
    GUARD(lock_);
    sender_options_ = opts;
}

void container::impl::receiver_options(const proton::receiver_options &opts) {
    GUARD(lock_);
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
        work_queue::impl* loop = connection_context::get(c).work_queue_.impl_.get();
        loop->run_all_jobs();
    }

    // Process events that shouldn't be sent to messaging_handler
    switch (pn_event_type(event)) {

    case PN_PROACTOR_INACTIVE: /* listener and all connections closed */
        // If we're stopping interrupt all other threads still running
        if (auto_stop_) pn_proactor_interrupt(proactor_);
        return false;

    // We only interrupt to stop threads
    case PN_PROACTOR_INTERRUPT:
        // Interrupt any other threads still running
        if (threads_>1) pn_proactor_interrupt(proactor_);
        return true;

    case PN_PROACTOR_TIMEOUT: {
        GUARD(lock_);
        // Can get an immediate timeout, if we have a container event loop inject
        if  ( deferred_.size()>0 ) {
            run_timer_jobs();
        }

        // Run every container event loop job
        // This is not at all efficient and single threads all these jobs, but it does correctly
        // serialise them
        for (work_queues::iterator loop = work_queues_.begin(); loop!=work_queues_.end(); ++loop) {
            (*loop)->run_all_jobs();
        }
        return false;
    }
    case PN_LISTENER_OPEN:
        return false;

    case PN_LISTENER_ACCEPT: {
        pn_listener_t* l = pn_event_listener(event);
        pn_connection_t* c = pn_connection();
        listener_context &lc(listener_context::get(l));
        pn_connection_set_container(c, id_.c_str());
        connection_options opts = server_connection_options_;
        if (lc.listen_handler_) {
            listener lstr(l);
            opts.update(lc.listen_handler_->on_accept(lstr));
        }
        else if (!!lc.connection_options_) opts.update(*lc.connection_options_);
        lc.connection_options_.reset(new connection_options(opts));
        // Handler applied separately
        connection_context& cc = connection_context::get(c);
        cc.container = &container_;
        cc.listener_context_ = &lc;
        cc.handler = opts.handler();
        cc.work_queue_ = new container::impl::connection_work_queue(*container_.impl_, c);
        pn_listener_accept(l, c);
        return false;
    }
    case PN_LISTENER_CLOSE: {
        pn_listener_t* l = pn_event_listener(event);
        listener_context &lc(listener_context::get(l));
        listener lstnr(l);
        if (lc.listen_handler_) {
            pn_condition_t* c = pn_listener_condition(l);
            if (pn_condition_is_set(c)) {
                lc.listen_handler_->on_error(lstnr, make_wrapper(c).what());
            }
            lc.listen_handler_->on_close(lstnr);
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

    messaging_adapter::dispatch(*mh, event);
    return false;
}

void container::impl::thread() {
    ++threads_;
    bool finished = false;
    do {
      pn_event_batch_t *events = pn_proactor_wait(proactor_);
      pn_event_t *e;
      try {
        while ((e = pn_event_batch_next(events))) {
          finished = handle(e);
          if (finished) break;
        }
      } catch (proton::error& e) {
        // If we caught an exception then shutdown the (other threads of the) container
        disconnect_error_ = error_condition("exception", e.what());
        if (!stopping_) stop(disconnect_error_);
        finished = true;
      } catch (...) {
        // If we caught an exception then shutdown the (other threads of the) container
        disconnect_error_ = error_condition("exception", "container shut-down by unknown exception");
        if (!stopping_) stop(disconnect_error_);
        finished = true;
      }
      pn_proactor_done(proactor_, events);
    } while(!finished);
    --threads_;
}

void container::impl::start_event() {
    if (handler_) handler_->on_container_start(container_);
}

void container::impl::stop_event() {
    if (handler_) handler_->on_container_stop(container_);
}

void container::impl::run(int threads) {
    // Have to "manually" generate container events
    CALL_ONCE(start_once_, &impl::start_event, this);

#if PN_CPP_SUPPORTS_THREADS
    // Run handler threads
    std::vector<std::thread> ts(threads-1);
    if (threads>1) {
      for (auto& t : ts) t = std::thread(&impl::thread, this);
    }

    thread();      // Use this thread too.

    // Wait for the other threads to stop
    if (threads>1) {
      for (auto& t : ts) t.join();
    }
#else
    // Run a single handler thread (As we have no threading API)
    thread();
#endif

    if (threads_==0) CALL_ONCE(stop_once_, &impl::stop_event, this);

    // Throw an exception if we disconnected the proactor because of an exception
    if (!disconnect_error_.empty()) {
      throw proton::error(disconnect_error_.description());
    };
}

void container::impl::auto_stop(bool set) {
    GUARD(lock_);
    auto_stop_ = set;
}

void container::impl::stop(const proton::error_condition& err) {
    GUARD(lock_);
    auto_stop_ = true;
    stopping_ = true;
    pn_condition_t* error_condition = pn_condition();
    set_error_condition(err, error_condition);
    pn_proactor_disconnect(proactor_, error_condition);
    pn_condition_free(error_condition);
}

}
