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

#include "proton/connection.hpp"
#include "proton/transport.hpp"
#include "proton/container.hpp"
#include "proton/event.hpp"
#include "proton/connection.h"
#include "connector.hpp"
#include "connection_impl.hpp"
#include "url.hpp"

namespace proton {

Connector::Connector(connection &c) : connection_(c), transport_(0) {}

Connector::~Connector() {}

void Connector::address(const std::string &a) {
    address_ = a;
}

void Connector::connect() {
    pn_connection_t *conn = connection_.pn_connection();
    pn_connection_set_container(conn, connection_.container().container_id().c_str());
    Url url(address_);
    std::string hostname = url.host() + ":" + url.port();
    pn_connection_set_hostname(conn, hostname.c_str());
    transport_ = new transport();
    transport_->bind(connection_);
    connection_.impl_->transport_ = transport_;
}


void Connector::on_connection_local_open(event &e) {
    connect();
}

void Connector::on_connection_remote_open(event &e) {}

void Connector::on_connection_init(event &e) {
}

void Connector::on_transport_closed(event &e) {
    // TODO: prepend with reconnect logic
    pn_connection_release(connection_.impl_->pn_connection_);
    // No more interaction, so drop our counted reference.
    connection_ = connection();
}


}
