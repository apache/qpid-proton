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
#include "proton/Container.hpp"
#include "proton/MessagingHandler.hpp"
#include "proton/Duration.hpp"
#include "proton/Error.hpp"
#include "proton/WaitCondition.hpp"
#include "BlockingConnectionImpl.hpp"
#include "Msg.hpp"
#include "contexts.hpp"

#include "proton/connection.h"

namespace proton {
namespace reactor {

WaitCondition::~WaitCondition() {}


void BlockingConnectionImpl::incref(BlockingConnectionImpl *impl) {
    impl->refCount++;
}

void BlockingConnectionImpl::decref(BlockingConnectionImpl *impl) {
    impl->refCount--;
    if (impl->refCount == 0)
        delete impl;
}

namespace {
struct ConnectionOpening : public WaitCondition {
    ConnectionOpening(pn_connection_t *c) : pnConnection(c) {}
    bool achieved() { return (pn_connection_state(pnConnection) & PN_REMOTE_UNINIT); }
    pn_connection_t *pnConnection;
};

struct ConnectionClosed : public WaitCondition {
    ConnectionClosed(pn_connection_t *c) : pnConnection(c) {}
    bool achieved() { return !(pn_connection_state(pnConnection) & PN_REMOTE_ACTIVE); }
    pn_connection_t *pnConnection;
};

}


BlockingConnectionImpl::BlockingConnectionImpl(std::string &u, Duration timeout0, SslDomain *ssld, Container *c)
    : url(u), timeout(timeout0), refCount(0)
{
    if (c)
        container = *c;
    container.start();
    container.setTimeout(timeout);
    // Create connection and send the connection events here
    connection = container.connect(url, static_cast<Handler *>(this));
    ConnectionOpening cond(connection.getPnConnection());
    wait(cond);
}

BlockingConnectionImpl::~BlockingConnectionImpl() {
    container = Container();
}

void BlockingConnectionImpl::close() {
    connection.close();
    ConnectionClosed cond(connection.getPnConnection());
    wait(cond);
}

void BlockingConnectionImpl::wait(WaitCondition &condition) {
    std::string empty;
    wait(condition, empty, timeout);
}

void BlockingConnectionImpl::wait(WaitCondition &condition, std::string &msg, Duration waitTimeout) {
    if (waitTimeout == Duration::FOREVER) {
        while (!condition.achieved()) {
            container.process();
        }
    }

    pn_reactor_t *reactor = container.getReactor();
    pn_millis_t origTimeout = pn_reactor_get_timeout(reactor);
    pn_reactor_set_timeout(reactor, waitTimeout.milliseconds);
    try {
        pn_timestamp_t now = pn_reactor_mark(reactor);
        pn_timestamp_t deadline = now + waitTimeout.milliseconds;
        while (!condition.achieved()) {
            container.process();
            if (deadline < pn_reactor_mark(reactor)) {
                std::string txt = "Connection timed out";
                if (!msg.empty())
                    txt += ": " + msg;
                // TODO: proper Timeout exception
                throw Error(MSG(txt));
            }
        }
    } catch (...) {
        pn_reactor_set_timeout(reactor, origTimeout);
        throw;
    }
    pn_reactor_set_timeout(reactor, origTimeout);
}



}} // namespace proton::reactor
