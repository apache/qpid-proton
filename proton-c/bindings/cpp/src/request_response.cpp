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
#include "proton/blocking_connection.hpp"
#include "proton/request_response.hpp"
#include "proton/event.hpp"
#include "proton/error.hpp"
#include "proton/value.hpp"
#include "blocking_connection_impl.hpp"
#include "msg.hpp"

namespace proton {

request_response::request_response(blocking_connection &conn, const std::string addr):
    connection_(conn), address_(addr),
    sender_(new blocking_sender(connection_, addr)),
    receiver_(new blocking_receiver(connection_, "", 1/*credit*/, true/*dynamic*/)),
    correlation_id_(0)
{}

message request_response::call(message &request) {
    if (address_.empty() && request.address().empty())
        throw error(MSG("Request message has no address"));
    // TODO: atomic increment.
    value cid(++correlation_id_);
    request.correlation_id(cid);
    request.reply_to(this->reply_to());
    sender_->send(request);
    message response;
    while (response.correlation_id() != cid) {
        response = receiver_->receive();
    }
    return response;
}

std::string request_response::reply_to() {
    return receiver_->receiver().remote_source().address();
}

}
