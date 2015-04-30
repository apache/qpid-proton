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

#include "proton/cpp/Connection.h"
#include "proton/cpp/Transport.h"
#include "proton/cpp/Container.h"
#include "proton/cpp/Event.h"
#include "proton/connection.h"
#include "Connector.h"
#include "ConnectionImpl.h"
#include "Url.h"
#include "LogInternal.h"

namespace proton {
namespace reactor {

Connector::Connector(Connection &c) : connection(c), transport(0) {}

Connector::~Connector() {}

void Connector::setAddress(const std::string &a) {
    address = a;
}

void Connector::connect() {
    pn_connection_t *conn = connection.getPnConnection();
    pn_connection_set_container(conn, connection.getContainer().getContainerId().c_str());
    Url url(address);
    std::string hostname = url.getHost() + ":" + url.getPort();
    pn_connection_set_hostname(conn, hostname.c_str());
    PN_CPP_LOG(info, "connecting to " << hostname << "...");
    transport = new Transport();
    transport->bind(connection);
    connection.impl->transport = transport;
}


void Connector::onConnectionLocalOpen(Event &e) {
    connect();
}

void Connector::onConnectionRemoteOpen(Event &e) {
    PN_CPP_LOG(info, "connected to " << e.getConnection().getHostname());
}

void Connector::onConnectionInit(Event &e) {

}

void Connector::onTransportClosed(Event &e) {
    // TODO: prepend with reconnect logic
    PN_CPP_LOG(info, "Disconnected");
    connection.setOverride(0);  // No more call backs
    pn_connection_release(connection.impl->pnConnection);
    delete this;
}


}} // namespace proton::reactor
