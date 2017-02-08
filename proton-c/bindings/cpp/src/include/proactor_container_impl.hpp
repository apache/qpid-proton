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

#include "proton_bits.hpp"
#include "proton_handler.hpp"

#include <list>
#include <map>
#include <string>
#include <vector>

struct pn_proactor_t;
struct pn_listener_t;
struct pn_event_t;

namespace proton {

class container::impl {
  public:
    impl(container& c, const std::string& id, messaging_handler* = 0);
    ~impl();
    std::string id() const { return id_; }
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
    void run();
    void stop(const error_condition& err);
    void auto_stop(bool set);
    void schedule(duration, void_function0&);
#if PN_CPP_HAS_STD_FUNCTION
    void schedule(duration, std::function<void()>);
#endif
    template <class T> static void set_handler(T s, messaging_handler* h);
    template <class T> static messaging_handler* get_handler(T s);

  private:
    pn_listener_t* listen_common_lh(const std::string&);
    connection connect_common(const std::string&, const connection_options&);

    // Event loop to run in each container thread
    static void thread(impl&);
    bool handle(pn_event_t*);
    void run_timer_jobs();

    container& container_;

    struct scheduled {
        timestamp time; // duration from epoch for task
#if PN_CPP_HAS_STD_FUNCTION
        std::function<void()>  task;
#else
        void_function0* task_;
        void task();
#endif

        // We want to get to get the *earliest* first so test is "reversed"
        bool operator < (const scheduled& r) const { return  r.time < time; }
    };
    std::vector<scheduled> deferred_; // This vector is kept as a heap

    pn_proactor_t* proactor_;
    messaging_handler* handler_;
    std::string id_;
    connection_options client_connection_options_;
    connection_options server_connection_options_;
    proton::sender_options sender_options_;
    proton::receiver_options receiver_options_;

    proton::error_condition stop_err_;
    bool auto_stop_;
    bool stopping_;
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
