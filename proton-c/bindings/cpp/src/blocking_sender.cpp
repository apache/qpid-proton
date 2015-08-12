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
#include "proton/blocking_sender.hpp"
#include "proton/blocking_connection.hpp"
#include "proton/sender.hpp"
#include "proton/receiver.hpp"
#include "proton/error.hpp"
#include "msg.hpp"


namespace proton {

namespace {
struct delivery_settled {
    delivery_settled(pn_delivery_t *d) : pn_delivery(d) {}
    bool operator()() { return pn_delivery_settled(pn_delivery); }
    pn_delivery_t *pn_delivery;
};

} // namespace


blocking_sender::blocking_sender(blocking_connection &c, sender l) : blocking_link(&c, l.get()) {
    std::string ta = link_.target().address();
    std::string rta = link_.remote_target().address();
    if (ta.empty() || ta.compare(rta) != 0) {
        wait_for_closed();
        link_.close();
        std::string txt = "Failed to open sender " + link_.name() + ", target does not match";
        throw error(MSG(txt));
    }
}

delivery blocking_sender::send(message &msg, duration timeout) {
    sender snd(link_.get());
    delivery dlv = snd.send(msg);
    std::string txt = "Sending on sender " + link_.name();
    delivery_settled cond(dlv.get());
    connection_.wait(cond, txt, timeout);
    return dlv;
}

delivery blocking_sender::send(message &msg) {
    // Use default timeout
    return send(msg, connection_.timeout());
}

}
