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
#include "proton/container.hpp"
#include "proton/blocking_connection.hpp"
#include "blocking_connection_impl.hpp"

namespace proton {

blocking_connection::blocking_connection(const proton::url &url, duration timeout) :
    impl_(new blocking_connection_impl(url, timeout))
{}

blocking_connection::~blocking_connection() {}

void blocking_connection::close() { impl_->close(); }

duration blocking_connection::timeout() const { return impl_->container_->reactor().timeout(); }

connection blocking_connection::connection() const { return impl_->connection_; }

}
