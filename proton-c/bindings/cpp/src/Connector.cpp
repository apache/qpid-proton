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

#include "Connector.h"
#include "proton/cpp/Connection.h"
#include "proton/cpp/Transport.h"
#include "proton/connection.h"

namespace proton {
namespace cpp {
namespace reactor {

Connector::Connector(Connection &c) : connection(c), transport(0) {}

Connector::~Connector() {}

void Connector::setAddress(const std::string &a) {
    address = a;
}

void Connector::connect() {
    // TODO: log("connecting to %s..." % connection.hostname)
    pn_connection_set_hostname(connection.getPnConnection(), address.c_str());
    transport = new Transport();
    transport->bind(connection);
    connection.transport = transport;
}


void Connector::onConnectionLocalOpen(Event &e) {
    connect();
}

void Connector::onConnectionRemoteOpen(Event &e) {
    // log("connected to %s\n", the_target)
}

void Connector::onConnectionInit(Event &e) {

}

void Connector::onTransportClosed(Event &e) {
    // TODO: reconnect code
}


}}} // namespace proton::cpp::reactor
