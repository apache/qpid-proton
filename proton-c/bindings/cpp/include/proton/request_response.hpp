#ifndef PROTON_CPP_SYNC_REQUEST_RESPONSE_H
#define PROTON_CPP_SYNC_REQUEST_RESPONSE_H

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
#include "proton/blocking_receiver.hpp"
#include "proton/blocking_sender.hpp"
#include "proton/export.hpp"
#include "proton/messaging_handler.hpp"
#include "proton/pn_unique_ptr.hpp"

#include <string>

struct pn_message_t;
struct pn_data_t;

namespace proton {

/**
 * An implementation of the synchronous request-response pattern (aka RPC).
 */
class request_response
{
  public:
    PN_CPP_EXTERN request_response(
        blocking_connection &, const std::string address=std::string());
    /**
     * Send a request message, wait for and return the response message.
     * Modifies the message to set `address` (if not already set), `reply_to` and `correlation_id`.
     */
    PN_CPP_EXTERN message call(message &request);
    /** Return the dynamic address of our receiver. */
    PN_CPP_EXTERN std::string reply_to();

  private:
    blocking_connection &connection_;
    std::string address_;
    pn_unique_ptr<blocking_sender> sender_;
    pn_unique_ptr<blocking_receiver> receiver_;
    amqp_ulong correlation_id_;
};

}

#endif  /*!PROTON_CPP_SYNC_REQUEST_RESPONSE_H*/
