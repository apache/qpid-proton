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

package org.apache.qpid.proton.example.reactor;

import java.io.IOException;

import org.apache.qpid.proton.Proton;
import org.apache.qpid.proton.amqp.messaging.AmqpValue;
import org.apache.qpid.proton.engine.BaseHandler;
import org.apache.qpid.proton.engine.Delivery;
import org.apache.qpid.proton.engine.Event;
import org.apache.qpid.proton.engine.Receiver;
import org.apache.qpid.proton.message.Message;
import org.apache.qpid.proton.reactor.FlowController;
import org.apache.qpid.proton.reactor.Handshaker;
import org.apache.qpid.proton.reactor.Reactor;

public class Recv extends BaseHandler {

    private Recv() {
        add(new Handshaker());
        add(new FlowController());
    }

    @Override
    public void onReactorInit(Event event) {
        try {
            // Create an amqp acceptor.
            event.getReactor().acceptor("0.0.0.0", 5672);

            // There is an optional third argument to the Reactor.acceptor
            // call. Using it, we could supply a handler here that would
            // become the handler for all accepted connections. If we omit
            // it, the reactor simply inherits all the connection events.
        } catch(IOException ioException) {
            ioException.printStackTrace();
        }
    }

    @Override
    public void onDelivery(Event event) {
        Receiver recv = (Receiver)event.getLink();
        Delivery delivery = recv.current();
        if (delivery.isReadable() && !delivery.isPartial()) {
            int size = delivery.pending();
            byte[] buffer = new byte[size];
            int read = recv.recv(buffer, 0, buffer.length);
            recv.advance();

            Message msg = Proton.message();
            msg.decode(buffer, 0, read);
            System.out.println(((AmqpValue)msg.getBody()).getValue());
        }
    }

    public static void main(String[] args) throws IOException {
        Reactor r = Proton.reactor(new Recv());
        r.run();
    }
}
