#ifndef REACTOR_HPP
#define REACTOR_HPP
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

#include "proton/facade.hpp"
#include "proton/duration.hpp"
#include "proton/pn_unique_ptr.hpp"

struct pn_reactor_t;

namespace proton {

class connection;
class acceptor;
class url;
class handler;

class reactor : public facade<pn_reactor_t, reactor> {
 public:
    /** Create a new reactor. */
    PN_CPP_EXTERN static pn_unique_ptr<reactor> create();

    /** Open a connection @see connection::open  */
    PN_CPP_EXTERN connection& connect(const proton::url&, handler *h=0);

    /** Open a connection to url and create a receiver with source=url.path() */
    PN_CPP_EXTERN acceptor& listen(const proton::url &);

    /** Run the event loop, return when all connections and acceptors are closed. */
    PN_CPP_EXTERN void run();

    /** Start the reactor, you must call process() to process events */
    PN_CPP_EXTERN void start();

    /** Process events, return true if there are more events to process. */
    PN_CPP_EXTERN bool process();

    /** Stop the reactor, causes run() to return and process() to return false. */
    PN_CPP_EXTERN void stop();

    /// Identifier for the container
    PN_CPP_EXTERN std::string id();

    /// Get timeout, process() will return if there is no activity within the timeout.
    PN_CPP_EXTERN duration timeout();

    /// Set timeout, process() will return if there is no activity within the timeout.
    PN_CPP_EXTERN void timeout(duration timeout);

    PN_CPP_EXTERN void wakeup();
    PN_CPP_EXTERN bool quiesced();
    PN_CPP_EXTERN void yield();

    void operator delete(void*);
};

}
#endif // REACTOR_HPP
