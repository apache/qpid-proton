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

#include "proton/listen_handler.hpp"

namespace proton {

container::~container() {}

/// Functions defined here are convenience overrides that can be trivially
/// defined in terms of other pure virtual functions on container. Don't make
/// container implementers wade thru all this boiler-plate.

returned<connection> standard_container::connect(const std::string &url) {
    return connect(url, connection_options());
}

returned<sender> standard_container::open_sender(const std::string &url) {
    return open_sender(url, proton::sender_options(), connection_options());
}

returned<sender> standard_container::open_sender(const std::string &url, const proton::sender_options &lo) {
    return open_sender(url, lo, connection_options());
}

returned<sender> standard_container::open_sender(const std::string &url, const proton::connection_options &co) {
    return open_sender(url, sender_options(), co);
}

returned<receiver> standard_container::open_receiver(const std::string &url) {
    return open_receiver(url, proton::receiver_options(), connection_options());
}

returned<receiver> standard_container::open_receiver(const std::string &url, const proton::receiver_options &lo) {
    return open_receiver(url, lo, connection_options());
}

returned<receiver> standard_container::open_receiver(const std::string &url, const proton::connection_options &co) {
    return open_receiver(url, receiver_options(), co);
}

namespace{
    struct listen_opts : public listen_handler {
        connection_options  opts;
        listen_opts(const connection_options& o) : opts(o) {}
        connection_options on_accept() { return opts; }
        void on_close() { delete this; }
    };
}

listener standard_container::listen(const std::string& url, const connection_options& opts) {
    // Note: listen_opts::on_close() calls delete(this) so this is not a leak.
    // The container will always call on_closed() even if there are errors or exceptions. 
    listen_opts* lh = new listen_opts(opts);
    return listen(url, *lh);
}

listener standard_container::listen(const std::string &url) {
    return listen(url, connection_options());
}

void standard_container::stop() { stop(error_condition()); }

} // namespace proton
