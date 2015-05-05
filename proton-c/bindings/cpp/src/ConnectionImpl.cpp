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
#include "proton/cpp/Container.h"
#include "proton/cpp/Handler.h"
#include "proton/cpp/exceptions.h"
#include "ConnectionImpl.h"
#include "proton/cpp/Transport.h"
#include "Msg.h"
#include "contexts.h"

#include "proton/connection.h"

namespace proton {
namespace reactor {

void ConnectionImpl::incref(ConnectionImpl *impl) {
    impl->refCount++;
}

void ConnectionImpl::decref(ConnectionImpl *impl) {
    impl->refCount--;
    if (impl->refCount == 0)
        delete impl;
}

ConnectionImpl::ConnectionImpl(Container &c) : container(c), refCount(0), override(0), transport(0), defaultSession(0),
                                               pnConnection(pn_reactor_connection(container.getReactor(), NULL)),
                                               reactorReference(this)
{
    setConnectionContext(pnConnection, this);
}

ConnectionImpl::~ConnectionImpl() {
    delete transport;
    delete override;
}

Transport &ConnectionImpl::getTransport() {
    if (transport)
        return *transport;
    throw ProtonException(MSG("Connection has no transport"));
}

Handler* ConnectionImpl::getOverride() { return override; }
void ConnectionImpl::setOverride(Handler *h) {
    if (override)
        delete override;
    override = h;
}

void ConnectionImpl::open() {
    pn_connection_open(pnConnection);
}

void ConnectionImpl::close() {
    pn_connection_close(pnConnection);
}

pn_connection_t *ConnectionImpl::getPnConnection() { return pnConnection; }

std::string ConnectionImpl::getHostname() {
    return std::string(pn_connection_get_hostname(pnConnection));
}

Connection &ConnectionImpl::getConnection() {
    // Endpoint interface.  Should be implemented in the Connection object.
    throw ProtonException(MSG("Internal error"));
}

Container &ConnectionImpl::getContainer() {
    return (container);
}

void ConnectionImpl::reactorDetach() {
    // "save" goes out of scope last, preventing possible recursive destructor
    // confusion with reactorReference.
    Connection save(reactorReference);
    if (reactorReference)
        reactorReference = Connection();
    pnConnection = 0;
}

Connection &ConnectionImpl::getReactorReference(pn_connection_t *conn) {
    if (!conn)
        throw ProtonException(MSG("Null Proton connection"));
    ConnectionImpl *impl = getConnectionContext(conn);
    if (!impl) {
        // First time we have seen this connection
        pn_reactor_t *reactor = pn_object_reactor(conn);
        if (!reactor)
            throw ProtonException(MSG("Invalid Proton connection specifier"));
        Container container(getContainerContext(reactor));
        if (!container)  // can't be one created by our container
            throw ProtonException(MSG("Unknown Proton connection specifier"));
        Connection connection(container);
        impl = connection.impl;
        setConnectionContext(conn, impl);
        impl->reactorReference = connection;
    }
    return impl->reactorReference;
}

}} // namespace proton::reactor
