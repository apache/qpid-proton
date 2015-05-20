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
#include "proton/cpp/BlockingSender.h"
#include "proton/cpp/BlockingConnection.h"
#include "proton/cpp/WaitCondition.h"
#include "proton/cpp/exceptions.h"
#include "Msg.h"


namespace proton {
namespace reactor {

namespace {
struct DeliverySettled : public WaitCondition {
    DeliverySettled(pn_delivery_t *d) : pnDelivery(d) {}
    bool achieved() { return pn_delivery_settled(pnDelivery); }
    pn_delivery_t *pnDelivery;
};

} // namespace


BlockingSender::BlockingSender(BlockingConnection &c, Sender &l) : BlockingLink(&c, l.getPnLink()) {
    std::string ta = link.getTarget().getAddress();
    std::string rta = link.getRemoteTarget().getAddress();
    if (ta.empty() || ta.compare(rta) != 0) {
        waitForClosed();
        link.close();
        std::string txt = "Failed to open sender " + link.getName() + ", target does not match";
        throw ProtonException(MSG("Container not started"));
    }
}

Delivery BlockingSender::send(Message &msg, Duration timeout) {
    Sender snd = link;
    Delivery dlv = snd.send(msg);
    std::string txt = "Sending on sender " + link.getName();
    DeliverySettled cond(dlv.getPnDelivery());
    connection.wait(cond, txt, timeout);
    return dlv;
}

Delivery BlockingSender::send(Message &msg) {
    // Use default timeout
    return send(msg, connection.getTimeout());
}

}} // namespace proton::reactor
