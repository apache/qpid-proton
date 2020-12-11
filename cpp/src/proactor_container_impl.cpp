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

#include "connect_config.hpp"
#include "proton/error_condition.hpp"
#include "proton/listen_handler.hpp"
#include "proton/listener.hpp"
#include "proton/reconnect_options.hpp"
#include "proton/ssl.hpp"
#include "proton/transport.hpp"
#include "proton/url.hpp"

#include "proton/connection.h"
#include "proton/listener.h"
#include "proton/proactor.h"
#include "proton/transport.h"

#include "contexts.hpp"
#include "messaging_adapter.hpp"
#include "reconnect_options_impl.hpp"
#include "proton_bits.hpp"

#include <assert.h>
#include <string.h>

#include <algorithm>
#include <vector>

#if PN_CPP_SUPPORTS_THREADS
# include <thread>
#endif

#if PN_CPP_HAS_HEADER_RANDOM
# include <random>
#endif

// XXXX: Debug
//#include <iostream>

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
    container_.schedule(d, make_work(&work_queue::impl::add_void, (work_queue::impl*)this, f));
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
      reconnecting_(0), auto_stop_(true), stopping_(false)
{}

container::impl::~impl() {
    pn_proactor_free(proactor_);
}

container::impl::container_work_queue* container::impl::add_work_queue() {
    container_work_queue* c = new container_work_queue(*this);
    GUARD(work_queues_lock_);
    work_queues_.insert(c);
    return c;
}

void container::impl::remove_work_queue(container::impl::container_work_queue* l) {
    GUARD(work_queues_lock_);
    work_queues_.erase(l);
}

namespace {
void default_url_options(connection_options& opts, const url& url) {
    opts.virtual_host(url.host());
    if (!url.user().empty())
        opts.user(url.user());
    if (!url.password().empty())
        opts.password(url.password());
    // If scheme is amqps then use default tls settings
    if (url.scheme()==url.AMQPS) {
        opts.ssl_client_options(ssl_client_options());
    }
}
}

pn_connection_t* container::impl::make_connection_lh(
    const url& url,
    const connection_options& user_opts)
{
    if (stopping_)
        throw proton::error("container is stopping");

    connection_options opts;
    opts.container_id(id_);
    default_url_options(opts, url);
    opts.update(client_connection_options_);
    opts.update(user_opts);
    messaging_handler* mh = opts.handler();

    pn_connection_t *pnc = pn_connection();
    connection_context& cc(connection_context::get(pnc));
    cc.container = &container_;
    cc.handler = mh;
    cc.work_queue_ = new container::impl::connection_work_queue(*container_.impl_, pnc);
    cc.reconnect_url_ = url;
    cc.connection_options_.reset(new connection_options(opts));

    make_wrapper(pnc).open(*cc.connection_options_);

    return pnc;                 // 1 refcount from pn_connection()
}

// Takes ownership of pnc
//
// NOTE: After the call to start_connection() pnc is active in a proactor thread,
// and may even have been freed already. It is undefined to use pnc (or any
// object belonging to it) except in appropriate handlers.
//
// SUBTLE NOTE: There must not be any proton::object wrappers in scope when
// start_connection() is called. The wrapper destructor will call pn_decref()
// after start_connection() which is undefined!
//
void container::impl::start_connection(const url& url, pn_connection_t *pnc) {
    pn_transport_t* pnt = pn_transport();
    connection_context& cc = connection_context::get(pnc);
    connection_options& co = *cc.connection_options_;
    co.apply_unbound_client(pnt);

    char caddr[PN_MAX_ADDR];
    pn_proactor_addr(caddr, sizeof(caddr), url.host().c_str(), url.port().c_str());
    pn_proactor_connect2(proactor_, pnc, pnt, caddr); // Takes ownership of pnc, pnt
}

void container::impl::reconnect(pn_connection_t* pnc) {
    --reconnecting_;

    if (stopping_ && reconnecting_==0) {
        pn_connection_free(pnc);
        //TODO: We've lost the error - we should really propagate it here
        pn_proactor_disconnect(proactor_, NULL);
        return;
    }

    connection_context& cc = connection_context::get(pnc);
    reconnect_context& rc = *cc.reconnect_context_.get();

    connection_options& co = *cc.connection_options_;
    co.apply_reconnect_urls(pnc);

    // Figure out next connection url to try
    // rc.current_url_ == -1 means try the url specified in connect, not a failover url
    const proton::url url(rc.current_url_==-1 ? cc.reconnect_url_ : cc.failover_urls_[rc.current_url_]);

    // XXXX Debug:
    //std::cout << "Retries: " << rc.retries_ << " Delay: " << rc.delay_ << " Trying: " << url << "@" << rc.current_url_ << std::endl;

    ++rc.current_url_;
    // Did we go through all the urls?
    if (rc.current_url_==int(cc.failover_urls_.size())) {
        rc.current_url_ = -1;
        ++rc.retries_;
    }

    connection_options opts;
    opts.container_id(id_);
    default_url_options(opts, url);
    opts.update(co);
    messaging_handler* mh = opts.handler();
    cc.handler = mh;

    make_wrapper(pnc).open(co);
    start_connection(url, pnc);
}

namespace {
#if PN_CPP_HAS_HEADER_RANDOM && PN_CPP_HAS_THREAD_LOCAL
duration random_between(duration min, duration max)
{
    static thread_local std::default_random_engine gen;
    std::uniform_int_distribution<duration::numeric_type> dist{min.milliseconds(), max.milliseconds()};
    return duration(dist(gen));
}
#else
duration random_between(duration, duration max)
{
    return max;
}
#endif

duration next_delay(reconnect_context& rc) {
    // If we've not retried before do it immediately
    if (rc.retries_==0) return duration(0);

    // If we haven't tried all failover urls yet this round do it immediately
    if (rc.current_url_!=-1) return duration(0);

    const reconnect_options_base& roi = rc.reconnect_options_;
    if (rc.retries_==1) {
        rc.delay_ = roi.delay;
    } else {
        rc.delay_ = std::min(roi.max_delay, rc.delay_ * roi.delay_multiplier);
    }
    return random_between(roi.delay, rc.delay_);
}

inline reconnect_context* get_reconnect_context(pn_connection_t* pnc) {
    return connection_context::get(pnc).reconnect_context_.get();
}

void reset_reconnect(pn_connection_t* pnc) {
    reconnect_context* rc = get_reconnect_context(pnc);

    if (!rc) return;

    rc->delay_ = 0;
    rc->retries_ = 0;
    // set retry to the initial url next
    rc->current_url_ = -1;
}

}

bool container::impl::can_reconnect(pn_connection_t* pnc) {
    reconnect_context* rc = get_reconnect_context(pnc);

    // If reconnect not enabled just fail
    if (!rc) return false;

    // Don't reconnect if we are locally closed, the application will
    // not expect a connection it closed to re-open.
    if (rc->stop_reconnect_) return false;

    // If container stopping don't try to reconnect
    // - we pretend to have set up a reconnect attempt so
    //   that the proactor disconnect will finish and we will exit
    //   the run loop without error.
    {
        GUARD(lock_);
        if (stopping_) return true;
    }

    const reconnect_options_base& roi = rc->reconnect_options_;

    pn_transport_t* t = pn_connection_transport(pnc);
    pn_condition_t* condition = pn_transport_condition(t);

    // If we failed to authenticate then don't reconnect any more and just fail
    if ( !strcmp(pn_condition_get_name(condition), "amqp:unauthorized-access") ) return false;

    // If too many reconnect attempts just fail
    if ( roi.max_attempts != 0 && rc->retries_ >= roi.max_attempts) {
        pn_condition_format(condition, "proton:io", "Too many reconnect attempts (%d)", rc->retries_);
        return false;
    }
    return true;
}

void container::impl::setup_reconnect(pn_connection_t* pnc) {
    connection_context& cc = connection_context::get(pnc);
    reconnect_context* rc = cc.reconnect_context_.get();
    if (!rc) return;

    rc->reconnected_ = true;

    // Recover connection from proactor
    pn_proactor_release_connection(pnc);

    // Figure out delay till next reconnect
    duration delay = next_delay(*rc);

    // Schedule reconnect - can do this on container work queue as no one can have the connection
    // now anyway
    schedule(delay, make_work(&container::impl::reconnect, this, pnc));
    ++reconnecting_;
}

returned<connection> container::impl::connect(
    const std::string& addr,
    const proton::connection_options& user_opts)
{
    proton::url url(addr);
    pn_connection_t* pnc = 0;
    {
        GUARD(lock_);
        pnc = make_connection_lh(url, user_opts);
    }
    start_connection(url, pnc); // See comment on start_connection
    return make_returned<proton::connection>(pnc);
}

returned<connection> container::impl::connect() {
    connection_options opts;
    std::string addr = apply_config(opts);
    return connect(addr, opts);
}

returned<sender> container::impl::open_sender(const std::string &urlstr, const proton::sender_options &o1, const connection_options &o2)
{
    proton::url url(urlstr);
    pn_link_t* pnl = 0;
    pn_connection_t* pnc = 0;
    {
        GUARD(lock_);
        proton::sender_options lopts(sender_options_);
        lopts.update(o1);
        pnc = make_connection_lh(url, o2);
        connection conn(make_wrapper(pnc));
        pnl = unwrap(conn.default_session().open_sender(url.path(), lopts));
    }
    start_connection(url, pnc); // See comment on start_connection
    return make_returned<sender>(pnl);  // Unsafe returned pointer
}

returned<receiver> container::impl::open_receiver(const std::string &urlstr, const proton::receiver_options &o1, const connection_options &o2) {
    proton::url url(urlstr);
    pn_link_t* pnl = 0;
    pn_connection_t* pnc = 0;
    {
        GUARD(lock_);
        proton::receiver_options lopts(receiver_options_);
        lopts.update(o1);
        pnc = make_connection_lh(url, o2);
        connection conn(make_wrapper(pnc));
        pnl = unwrap(conn.default_session().open_receiver(url.path(), lopts));
    }
    start_connection(url, pnc); // See comment on start_connection
    return make_returned<receiver>(pnl);
}

pn_listener_t* container::impl::listen_common_lh(const std::string& addr) {
    if (stopping_)
        throw proton::error("container is stopping");

    proton::url url(addr, false); // Don't want un-helpful defaults like "localhost"

    // Figure out correct string len then create connection address
    int len = pn_proactor_addr(0, 0, url.host().c_str(), url.port().c_str());
    std::vector<char> caddr(len+1);
    pn_proactor_addr(&caddr[0], len+1, url.host().c_str(), url.port().c_str());

    pn_listener_t* listener = pn_listener();
    pn_listener_set_context(listener, &container_);
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
    GUARD(deferred_lock_);
    timestamp now = timestamp::now();

    // Record timeout; Add callback to timeout sorted list
    scheduled s = {now+delay, f};
    deferred_.push_back(s);
    std::push_heap(deferred_.begin(), deferred_.end());

    // Set timeout for current head of timeout queue
    scheduled* next = &deferred_.front();
    pn_millis_t timeout_ms = (now < next->time) ? (next->time-now).milliseconds() : 0;
    pn_proactor_set_timeout(proactor_, timeout_ms);
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
    timestamp now = timestamp::now();
    std::vector<scheduled> tasks;

    // We first extract all the runnable tasks and then run them -  this is to avoid having tasks
    // injected as we are running them (which could potentially never end)
    {
        GUARD(deferred_lock_);

        // Figure out how many tasks we need to execute and pop them to the back of the
        // queue (in reverse order)
        unsigned i = 0;
        for (;;) {
            // Have we seen all the queued tasks?
            if  ( deferred_.size()-i==0 ) break;

            // Is the next task in the future?
            timestamp next_time = deferred_.front().time;
            if ( next_time>now ) {
                pn_proactor_set_timeout(proactor_, (next_time-now).milliseconds());
                break;
            }

            std::pop_heap(deferred_.begin(), deferred_.end()-i);
            ++i;
        }
        // Nothing to do
        if ( i==0 ) return;

        // Now we know how many tasks to run
        if ( deferred_.size()==i ) {
            // If we sorted the entire heap, then we're executing every task
            // so don't need to copy and can just swap
            tasks.swap(deferred_);
        } else {
            // Otherwise just copy the ones we sorted
            tasks = std::vector<scheduled>(deferred_.end()-i, deferred_.end());

            // Remove tasks to be executed
            deferred_.resize(deferred_.size()-i);
        }
    }
    // We've now taken the tasks to run from the deferred tasks
    // so we can run them unlocked
    // NB. We copied the due tasks in reverse order so execute from end
    for (int i = tasks.size()-1; i>=0; --i) tasks[i].task();
}

// Return true if this thread is finished
container::impl::dispatch_result container::impl::dispatch(pn_event_t* event) {

    // If we have any pending connection work, do it now
    pn_connection_t* c = pn_event_connection(event);
    if (c) {
        work_queue::impl* queue = connection_context::get(c).work_queue_.impl_.get();
        queue->run_all_jobs();
    }

    // Process events that shouldn't be sent to messaging_handler
    switch (pn_event_type(event)) {

    case PN_PROACTOR_INACTIVE: /* listener and all connections closed */
        // If we're stopping interrupt all other threads still running
        if (auto_stop_) pn_proactor_interrupt(proactor_);
        return ContinueLoop;

    // We only interrupt to stop threads
    case PN_PROACTOR_INTERRUPT: {
        // Interrupt any other threads still running
        GUARD(lock_);
        if (threads_>1) pn_proactor_interrupt(proactor_);
        return EndLoop;
    }

    case PN_PROACTOR_TIMEOUT: {
        // Can get an immediate timeout, if we have a container event loop inject
        run_timer_jobs();

        // Run every container event loop job
        // This is not at all efficient and single threads all these jobs, but it does correctly
        // serialise them
        work_queues queues;
        {
            GUARD(work_queues_lock_);
            queues = work_queues_;
        }
        for (work_queues::iterator queue = queues.begin(); queue!=queues.end(); ++queue) {
            (*queue)->run_all_jobs();
        }
        return EndBatch;
    }
    case PN_LISTENER_OPEN: {
        pn_listener_t* l = pn_event_listener(event);
        proton::listen_handler* handler;
        {
            GUARD(lock_);
            listener_context &lc(listener_context::get(l));
            handler = lc.listen_handler_;
        }
        if (handler) {
            listener lstnr(l);
            handler->on_open(lstnr);
        }
        return ContinueLoop;
    }
    case PN_LISTENER_ACCEPT: {
        pn_listener_t* l = pn_event_listener(event);
        pn_connection_t* c = pn_connection();
        pn_connection_set_container(c, id_.c_str());
        connection_options opts = server_connection_options_;
        listen_handler* handler;
        listener_context* lc;
        const connection_options* options;
        {
            GUARD(lock_);
            lc = &listener_context::get(l);
            handler = lc->listen_handler_;
            options = lc->connection_options_.get();
        }
        if (handler) {
            listener lstr(l);
            opts.update(handler->on_accept(lstr));
        }
        else if (options) opts.update(*options);
        // Handler applied separately
        connection_context& cc = connection_context::get(c);
        cc.container = &container_;
        cc.listener_context_ = lc;
        cc.handler = opts.handler();
        cc.work_queue_ = new container::impl::connection_work_queue(*container_.impl_, c);
        pn_transport_t* pnt = pn_transport();
        pn_transport_set_server(pnt);
        opts.apply_unbound_server(pnt);
        pn_listener_accept2(l, c, pnt);
        return ContinueLoop;
    }
    case PN_LISTENER_CLOSE: {
        pn_listener_t* l = pn_event_listener(event);
        proton::listen_handler* handler;
        {
            GUARD(lock_);
            listener_context &lc(listener_context::get(l));
            handler = lc.listen_handler_;
        }
        listener lstnr(l);
        if (handler) {
            pn_condition_t* c = pn_listener_condition(l);
            if (pn_condition_is_set(c)) {
                handler->on_error(lstnr, make_wrapper(c).what());
            }
            handler->on_close(lstnr);
        }
        return ContinueLoop;
    }
    // Connection driver will bind a new transport to the connection at this point
    case PN_CONNECTION_INIT:
        return ContinueLoop;

    case PN_CONNECTION_REMOTE_OPEN: {
        // This is the only event that we get indicating that the connection succeeded so
        // it's the only place to reset the reconnection logic.
        //
        // Just note we have a connection then process normally
        pn_connection_t* c = pn_event_connection(event);
        reset_reconnect(c);
        break;
    }
    case PN_CONNECTION_REMOTE_CLOSE: {
        pn_connection_t *c = pn_event_connection(event);
        pn_condition_t *cc = pn_connection_remote_condition(c);

        // If reconnect is on, amqp:connection:forced should be treated specially:
        // Hide the connection error/close events from the application;
        // Then we close the connection noting the forced close;
        // Then set up for reconnect handling.
        if (get_reconnect_context(c) &&
            pn_condition_is_set(cc) &&
            !strcmp(pn_condition_get_name(cc), "amqp:connection:forced"))
        {
            pn_transport_t* t = pn_event_transport(event);
            pn_condition_t* tc = pn_transport_condition(t);
            pn_condition_copy(tc, cc);
            pn_transport_close_tail(t);
            pn_connection_close(c);
            return ContinueLoop;
        }
        break;
    }
    case PN_TRANSPORT_CLOSED: {
        // If reconnect is turned on then handle closed on error here with reconnect attempt
        pn_connection_t* c = pn_event_connection(event);
        pn_transport_t* t = pn_event_transport(event);
        if (pn_condition_is_set(pn_transport_condition(t)) && can_reconnect(c)) {
            messaging_handler *mh = get_handler(event);
            if (mh) {           // Notify handler of pending reconnect
                transport trans = make_wrapper(t);
                try {
                    mh->on_transport_error(trans);
                } catch (const proton::error& e) {
                    // If this is the same error we are re-connecting for,
                    // ignore it.  It was probably thrown by the default
                    // messaging_handler::on_error(), and if not the user has
                    // already seen it.
                    //
                    // If this isn't the same error, then something unexpected
                    // has happened, so re-throw.
                    if (std::string(e.what()) != trans.error().what())
                        throw;
                }
            }
            // on_transport_error() may have closed the connection, check again.
            reconnect_context* rc = get_reconnect_context(c);
            if (rc && !(rc->stop_reconnect_)) {
                setup_reconnect(c);
                return ContinueLoop;
            }
        }
        // Otherwise, this connection will be freed by the proactor.
        // Mark its work_queue finished so it won't try to use the freed connection.
        connection_context::get(c).work_queue_.impl_.get()->finished();
        break;
    }
    default:
        break;
    }

    messaging_handler *mh = get_handler(event);
    if (mh) messaging_adapter::dispatch(*mh, event);
    return ContinueLoop;
}

// Figure out the handler for the primary object for event
messaging_handler* container::impl::get_handler(pn_event_t *event) {
    messaging_handler *mh = 0;

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
    return mh ? mh : handler_;
}

void container::impl::thread() {
    bool finished;
    {
        GUARD(lock_);
        ++threads_;
        finished = stopping_;
    }
    while (!finished) {
        pn_event_batch_t *events = pn_proactor_wait(proactor_);
        pn_event_t *e;
        error_condition error;
        try {
            while ((e = pn_event_batch_next(events))) {
                dispatch_result r = dispatch(e);
                finished = r==EndLoop;
                if (r!=ContinueLoop) break;
            }
        } catch (const std::exception& e) {
            // If we caught an exception then shutdown the (other threads of the) container
            error = error_condition("exception", e.what());
        } catch (...) {
            error = error_condition("exception", "container shut-down by unknown exception");
        }
        pn_proactor_done(proactor_, events);
        if (!error.empty()) {
            finished = true;
            {
                GUARD(lock_);
                disconnect_error_ = error;
            }
            stop(error);
        }
    }
    {
        GUARD(lock_);
        --threads_;
    }
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
    threads = std::max(threads, 1); // Ensure at least 1 thread
    typedef std::vector<std::thread*> vt; // pointer vector to work around failures in older compilers
    vt ts(threads-1);
    for (vt::iterator i = ts.begin(); i != ts.end(); ++i) {
        *i = new std::thread(&impl::thread, this);
    }

    thread();      // Use this thread too.

    // Wait for the other threads to stop
    for (vt::iterator i = ts.begin(); i != ts.end(); ++i) {
        (*i)->join();
        delete *i;
    }
#else
    // Run a single handler thread (As we have no threading API)
    thread();
#endif

    bool last = false;
    {
        GUARD(lock_);
        last =  threads_==0;
    }
    if (last) CALL_ONCE(stop_once_, &impl::stop_event, this);

    // Throw an exception if we disconnected the proactor because of an exception
    {
        GUARD(lock_);
        if (!disconnect_error_.empty()) throw proton::error(disconnect_error_.description());
    };
}

void container::impl::auto_stop(bool set) {
    GUARD(lock_);
    auto_stop_ = set;
}

void container::impl::stop(const proton::error_condition& err) {
    {
        GUARD(lock_);
        if (stopping_) return;  // Already stopping
        auto_stop_ = true;
        stopping_ = true;
        // Have to wait until actual reconnect to stop or we leak the connection
        if (reconnecting_>0) return;
    }
    pn_condition_t* error_condition = pn_condition();
    set_error_condition(err, error_condition);
    pn_proactor_disconnect(proactor_, error_condition);
    pn_condition_free(error_condition);
}

}
