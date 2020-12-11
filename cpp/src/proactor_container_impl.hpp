#ifndef PROTON_CPP_PROACTOR_CONTAINERIMPL_H
#define PROTON_CPP_PROACTOR_CONTAINERIMPL_H

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

#include "proton/fwd.hpp"
#include "proton/container.hpp"
#include "proton/connection.hpp"
#include "proton/connection_options.hpp"
#include "proton/duration.hpp"
#include "proton/error_condition.hpp"
#include "proton/messaging_handler.hpp"
#include "proton/receiver.hpp"
#include "proton/receiver_options.hpp"
#include "proton/sender.hpp"
#include "proton/sender_options.hpp"
#include "proton/work_queue.hpp"

#include "proton_bits.hpp"

#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#if PN_CPP_SUPPORTS_THREADS
#include <mutex>
# define MUTEX(x) std::mutex x;
# define GUARD(x) std::lock_guard<std::mutex> g(x)
# define ONCE_FLAG(x) std::once_flag x;
# define CALL_ONCE(x, ...) std::call_once(x, __VA_ARGS__)
#else
# define MUTEX(x)
# define GUARD(x)
# define ONCE_FLAG(x)
# define CALL_ONCE(x, f, o) ((o)->*(f))()
#endif

struct pn_proactor_t;
struct pn_listener_t;
struct pn_event_t;

namespace proton {

namespace internal {
class connector;
}

class container::impl {
  public:
    impl(container& c, const std::string& id, messaging_handler* = 0);
    ~impl();
    std::string id() const { return id_; }
    returned<connection> connect();
    returned<connection> connect(const std::string&, const connection_options&);
    returned<sender> open_sender(
        const std::string&, const proton::sender_options &, const connection_options &);
    returned<receiver> open_receiver(
        const std::string&, const proton::receiver_options &, const connection_options &);
    listener listen(const std::string&);
    listener listen(const std::string&, const connection_options& lh);
    listener listen(const std::string&, listen_handler& lh);
    void client_connection_options(const connection_options &);
    connection_options client_connection_options() const { return client_connection_options_; }
    void server_connection_options(const connection_options &);
    connection_options server_connection_options() const { return server_connection_options_; }
    void sender_options(const proton::sender_options&);
    class sender_options sender_options() const { return sender_options_; }
    void receiver_options(const proton::receiver_options&);
    class receiver_options receiver_options() const { return receiver_options_; }
    void run(int threads);
    void stop(const error_condition& err);
    void auto_stop(bool set);
    void schedule(duration, work);
    template <class T> static void set_handler(T s, messaging_handler* h);
    template <class T> static messaging_handler* get_handler(T s);
    messaging_handler* get_handler(pn_event_t *event);
    static work_queue::impl* make_work_queue(container&);

  private:
    class common_work_queue;
    class connection_work_queue;
    class container_work_queue;
    pn_listener_t* listen_common_lh(const std::string&);
    pn_connection_t* make_connection_lh(const url& url, const connection_options&);
    void start_connection(const url& url, pn_connection_t* c);
    void reconnect(pn_connection_t* pnc);
    bool can_reconnect(pn_connection_t* pnc);
    void setup_reconnect(pn_connection_t* pnc);

    // Event loop to run in each container thread
    void thread();
    enum dispatch_result {ContinueLoop, EndBatch, EndLoop};
    dispatch_result dispatch(pn_event_t*);
    void run_timer_jobs();

    int threads_;
    container& container_;
    MUTEX(lock_)

    ONCE_FLAG(start_once_)
    ONCE_FLAG(stop_once_)
    void start_event();
    void stop_event();

    typedef std::set<container_work_queue*> work_queues;
    work_queues work_queues_;
    MUTEX(work_queues_lock_)
    container_work_queue* add_work_queue();
    void remove_work_queue(container_work_queue*);

    struct scheduled {
        timestamp time; // duration from epoch for task
        work task;

        // We want to get to get the *earliest* first so test is "reversed"
        bool operator < (const scheduled& r) const { return  r.time < time; }
    };
    std::vector<scheduled> deferred_; // This vector is kept as a heap
    MUTEX(deferred_lock_)

    pn_proactor_t* proactor_;
    messaging_handler* handler_;
    std::string id_;
    connection_options client_connection_options_;
    connection_options server_connection_options_;
    proton::sender_options sender_options_;
    proton::receiver_options receiver_options_;
    error_condition disconnect_error_;

    unsigned reconnecting_;
    bool auto_stop_;
    bool stopping_;
    friend class connector;
};

template <class T>
void container::impl::set_handler(T s, messaging_handler* mh) {
    internal::set_messaging_handler(s, mh);
}

template <class T>
messaging_handler* container::impl::get_handler(T s) {
    return internal::get_messaging_handler(s);
}


}

#endif  /*!PROTON_CPP_PROACTOR_CONTAINERIMPL_H*/
