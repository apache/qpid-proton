#ifndef PROTON_CPP_CONTAINERIMPL_H
#define PROTON_CPP_CONTAINERIMPL_H

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

#include "proton/io/link_namer.hpp"

#include "proton/container.hpp"
#include "proton/connection.hpp"
#include "proton/connection_options.hpp"
#include "proton/duration.hpp"
#include "proton/sender.hpp"
#include "proton/receiver.hpp"

#include "messaging_adapter.hpp"
#include "reactor.hpp"
#include "proton_bits.hpp"
#include "proton_handler.hpp"

#include <list>
#include <map>
#include <string>

namespace proton {

class dispatch_helper;
class connection;
class connector;
class acceptor;
class container;
class url;
class task;
class listen_handler;

class container_impl : public standard_container {
  public:
    // Pull in base class functions here so that name search finds all the overloads
    using standard_container::stop;
    using standard_container::connect;
    using standard_container::listen;
    using standard_container::open_receiver;
    using standard_container::open_sender;

    container_impl(const std::string& id, messaging_handler* = 0);
    ~container_impl();
    std::string id() const PN_CPP_OVERRIDE { return id_; }
    returned<connection> connect(const std::string&, const connection_options&) PN_CPP_OVERRIDE;
    returned<sender> open_sender(
        const std::string&, const proton::sender_options &, const connection_options &) PN_CPP_OVERRIDE;
    returned<receiver> open_receiver(
        const std::string&, const proton::receiver_options &, const connection_options &) PN_CPP_OVERRIDE;
    listener listen(const std::string&, listen_handler& lh) PN_CPP_OVERRIDE;
    void stop_listening(const std::string&) PN_CPP_OVERRIDE;
    void client_connection_options(const connection_options &) PN_CPP_OVERRIDE;
    connection_options client_connection_options() const PN_CPP_OVERRIDE { return client_connection_options_; }
    void server_connection_options(const connection_options &) PN_CPP_OVERRIDE;
    connection_options server_connection_options() const PN_CPP_OVERRIDE { return server_connection_options_; }
    void sender_options(const proton::sender_options&) PN_CPP_OVERRIDE;
    class sender_options sender_options() const PN_CPP_OVERRIDE { return sender_options_; }
    void receiver_options(const proton::receiver_options&) PN_CPP_OVERRIDE;
    class receiver_options receiver_options() const PN_CPP_OVERRIDE { return receiver_options_; }
    void run() PN_CPP_OVERRIDE;
    void stop(const error_condition& err) PN_CPP_OVERRIDE;
    void auto_stop(bool set) PN_CPP_OVERRIDE;
#if PN_CPP_HAS_STD_FUNCTION
    void schedule(duration, std::function<void()>) PN_CPP_OVERRIDE;
#endif
    void schedule(duration, void_function0&) PN_CPP_OVERRIDE;

    // non-interface functions
    void configure_server_connection(connection &c);
    static task schedule(container& c, int delay, proton_handler *h);
    template <class T> static void set_handler(T s, messaging_handler* h);

  private:
    internal::pn_ptr<pn_handler_t> cpp_handler(proton_handler *h);

    typedef std::map<std::string, acceptor> acceptors;

    reactor reactor_;
    // Keep a list of all the handlers used by the container so they last as long as the container
    std::list<internal::pn_unique_ptr<proton_handler> > handlers_;
    std::string id_;
    connection_options client_connection_options_;
    connection_options server_connection_options_;
    proton::sender_options sender_options_;
    proton::receiver_options receiver_options_;
    bool auto_stop_;
    acceptors acceptors_;

  friend class messaging_adapter;
};

template <class T>
void container_impl::set_handler(T s, messaging_handler* mh) {
    pn_record_t *record = internal::get_attachments(unwrap(s));
    proton_handler* h = new messaging_adapter(*mh);
    container_impl& ci = static_cast<container_impl&>(s.container());
    ci.handlers_.push_back(h);
    pn_record_set_handler(record, ci.cpp_handler(h).get());
}

}

#endif  /*!PROTON_CPP_CONTAINERIMPL_H*/
