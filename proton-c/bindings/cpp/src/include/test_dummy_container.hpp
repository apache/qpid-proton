#ifndef TEST_DUMMY_CONTAINER_HPP
#define TEST_DUMMY_CONTAINER_HPP

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

#include "proton/container.hpp"
#include "proton/event_loop.hpp"
#include "proton/thread_safe.hpp"

namespace test {

using namespace proton;


class dummy_container : public standard_container {
  public:
    dummy_container(const std::string cid="") :
        id_(cid), fail("not implemented for dummy_container") {}

    // Pull in base class functions here so that name search finds all the overloads
    using standard_container::stop;
    using standard_container::connect;
    using standard_container::listen;
    using standard_container::open_receiver;
    using standard_container::open_sender;

    returned<connection> connect(const std::string&, const connection_options&) { throw fail; }
    listener listen(const std::string& , listen_handler& ) { throw fail; }
    void stop_listening(const std::string&) { throw fail; }
    void run() { throw fail; }
    void auto_stop(bool) { throw fail; }
    void stop(const proton::error_condition& ) { throw fail; }
    returned<sender> open_sender(const std::string &, const proton::sender_options &, const connection_options&) { throw fail; }
    returned<receiver> open_receiver( const std::string &, const proton::receiver_options &, const connection_options &) { throw fail; }
    std::string id() const { return id_; }
    void client_connection_options(const connection_options &o) { ccopts_ = o; }
    connection_options client_connection_options() const { return ccopts_; }
    void server_connection_options(const connection_options &o) { scopts_ = o; }
    connection_options server_connection_options() const { return scopts_; }
    void sender_options(const class sender_options &o) { sopts_ = o; }
    class sender_options sender_options() const { return sopts_; }
    void receiver_options(const class receiver_options &o) { ropts_ = o; }
    class receiver_options receiver_options() const { return ropts_; }
#if PN_CPP_HAS_STD_FUNCTION
    void schedule(duration, std::function<void()>) { throw fail; }
#endif
    void schedule(duration, void_function0&) { throw fail; }

  private:
    std::string id_;
    connection_options ccopts_, scopts_;
    class sender_options sopts_;
    class receiver_options ropts_;
    std::runtime_error fail;
};

class dummy_event_loop : public event_loop {
#if PN_CPP_HAS_CPP11
    bool inject(std::function<void()> f) PN_CPP_OVERRIDE { f(); return true; }
#endif
    bool inject(proton::void_function0& h) PN_CPP_OVERRIDE { h(); return true; }
};

}

#endif // TEST_DUMMY_CONTAINER_HPP
