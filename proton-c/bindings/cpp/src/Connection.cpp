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
#include "proton/cpp/Connection.h"
#include "proton/cpp/Handler.h"
#include "contexts.h"

#include "proton/connection.h"

namespace proton {
namespace cpp {
namespace reactor {


Connection::Connection(Container &c) : container(c), override(0), transport(0), defaultSession(0),
                                       pnConnection(pn_reactor_connection(container.getReactor(), NULL))
{
    setConnectionContext(pnConnection, this);
}

Connection::~Connection(){}

Transport &Connection::getTransport() {
    if (transport)
        return *transport;
    throw "TODO: real no transport exception here";
}

Handler* Connection::getOverride() { return override; }
void Connection::setOverride(Handler *h) { override = h; }

void Connection::open() {
    pn_connection_open(pnConnection);
}

void Connection::close() {
    pn_connection_close(pnConnection);
}

pn_connection_t *Connection::getPnConnection() { return pnConnection; }

std::string Connection::getHostname() { 
    return std::string(pn_connection_get_hostname(pnConnection));
}

Connection &Connection::getConnection() {
    return (*this);
}

}}} // namespace proton::cpp::reactor
