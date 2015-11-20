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
#include "proton/url.hpp"

#include "connector.hpp"

namespace proton {

connector::connector(connection &c) : connection_(c) {}

connector::~connector() {}

void connector::address(const url &a) {
    address_ = a;
}

void connector::connect() {
    connection_.container_id(connection_.container().id());
    connection_.host(address_.host_port());
}

void connector::on_connection_local_open(event &e) {
    connect();
}

void connector::on_connection_remote_open(event &e) {}

void connector::on_connection_init(event &e) {
}

void connector::on_transport_closed(event &e) {
    // TODO: prepend with reconnect logic
    connection_.release();
    connection_  = 0;
}

}
