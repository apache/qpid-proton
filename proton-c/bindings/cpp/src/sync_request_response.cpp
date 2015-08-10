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
#include "proton/sync_request_response.hpp"
#include "proton/event.hpp"
#include "proton/error.hpp"
#include "msg.hpp"

namespace proton {

namespace {
amqp_ulong global_correlation_id = 0;

struct response_received {
    response_received(std::auto_ptr<message>& m, amqp_ulong id) : message_(m), id_(id) {}
    bool operator()() { return message_.get() && message_->correlation_id() == id_; }
    std::auto_ptr<message>& message_;
    value id_;
};

}

sync_request_response::sync_request_response(blocking_connection &conn, const std::string addr):
    connection_(conn), address_(addr),
    sender_(connection_.create_sender(addr)),
    receiver_(connection_.create_receiver("", 1/*credit*/, true/*dynamic*/, this))
{
}

message sync_request_response::call(message &request) {
    if (address_.empty() && request.address().empty())
        throw error(MSG("Request message has no address"));
    // TODO: thread safe increment.
    correlation_id_ = global_correlation_id++;
    request.correlation_id(value(correlation_id_));
    request.reply_to(this->reply_to());
    sender_.send(request);
    std::string txt("Waiting for response");
    response_received cond(response_, correlation_id_);
    connection_.wait(cond, txt);
    message resp = *response_;
    response_.reset(0);
    receiver_.flow(1);
    return resp;
}

std::string sync_request_response::reply_to() {
    return receiver_.remote_source().address();
}

void sync_request_response::on_message(event &e) {
    response_.reset(new message);
    response_->swap(e.message());
    // Wake up enclosing blocking_connection.wait() to handle the message
    e.container().yield();
}


} // namespace
