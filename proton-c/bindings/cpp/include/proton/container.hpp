#ifndef PROTON_CPP_CONTAINER_H
#define PROTON_CPP_CONTAINER_H

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
#include "proton/export.hpp"
#include "proton/handle.hpp"
#include "proton/acceptor.hpp"
#include "proton/duration.hpp"
#include "proton/url.hpp"
#include <proton/reactor.h>
#include <string>

namespace proton {

class dispatch_helper;
class connection;
class connector;
class acceptor;
class container_impl;
class messaging_handler;
class sender;
class receiver;
class link;
class handler;

/** Top level container for connections and other objects, runs the event loop */ 
class container : public handle<container_impl>
{
  public:
    ///@name internal @internal @{
    PN_CPP_EXTERN container(container_impl *);
    PN_CPP_EXTERN container(const container& c);
    PN_CPP_EXTERN container& operator=(const container& c);
    PN_CPP_EXTERN ~container();
    ///@}

    /** Create a container */
    PN_CPP_EXTERN container();

    /** Create a container and set the top-level messaging_handler */
    PN_CPP_EXTERN container(messaging_handler &mhandler);

    /** Locally open a connection @see connection::open  */
    PN_CPP_EXTERN connection connect(const proton::url&, handler *h=0);

    /** Run the event loop, return when all connections and acceptors are closed. */
    PN_CPP_EXTERN void run();

    /** Start the reactor, you must call process() to process events */
    PN_CPP_EXTERN void start();

    /** Process events, return true if there are more events to process. */
    PN_CPP_EXTERN bool process();

    /** Stop the reactor, causes run() to return and process() to return false. */
    PN_CPP_EXTERN void stop();

    /** Create a sender on connection with target=addr and optional handler h */
    PN_CPP_EXTERN sender create_sender(connection &connection, const std::string &addr, handler *h=0);

    /** Open a connection to url and create a sender with target=url.path() */
    PN_CPP_EXTERN sender create_sender(const proton::url &);

    /** Create a receiver on connection with target=addr and optional handler h */
    PN_CPP_EXTERN receiver create_receiver(connection &connection, const std::string &addr, bool dynamic=false, handler *h=0);

    /** Create a receiver on connection with source=url.path() */
    PN_CPP_EXTERN receiver create_receiver(const url &);

    /** Open a connection to url and create a receiver with source=url.path() */
    PN_CPP_EXTERN acceptor listen(const proton::url &);

    /// Identifier for the container
    PN_CPP_EXTERN std::string container_id();

    /// Get timeout, process() will return if there is no activity within the timeout.
    PN_CPP_EXTERN duration timeout();

    /// Set timeout, process() will return if there is no activity within the timeout.
    PN_CPP_EXTERN void timeout(duration timeout);

    /// Get the reactor
    PN_CPP_EXTERN pn_reactor_t *reactor();

    PN_CPP_EXTERN void wakeup();
    PN_CPP_EXTERN bool is_quiesced();
    PN_CPP_EXTERN void yield();
private:
   friend class private_impl_ref<container>;
};

}

#endif  /*!PROTON_CPP_CONTAINER_H*/
