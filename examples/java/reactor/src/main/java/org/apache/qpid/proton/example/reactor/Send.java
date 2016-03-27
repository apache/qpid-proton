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
import java.nio.BufferOverflowException;

import org.apache.qpid.proton.Proton;
import org.apache.qpid.proton.amqp.messaging.AmqpValue;
import org.apache.qpid.proton.amqp.transport.ErrorCondition;
import org.apache.qpid.proton.engine.BaseHandler;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Delivery;
import org.apache.qpid.proton.engine.Event;
import org.apache.qpid.proton.engine.Sender;
import org.apache.qpid.proton.engine.Session;
import org.apache.qpid.proton.message.Message;
import org.apache.qpid.proton.reactor.Handshaker;
import org.apache.qpid.proton.reactor.Reactor;

// This is a send in terms of low level AMQP events.
public class Send extends BaseHandler {

    private class SendHandler extends BaseHandler {

        private final Message message;
        private int nextTag = 0;

        private SendHandler(Message message) {
            this.message = message;

            // Add a child handler that performs some default handshaking
            // behaviour.
            add(new Handshaker());
        }

        @Override
        public void onConnectionInit(Event event) {
            Connection conn = event.getConnection();

            // Every session or link could have their own handler(s) if we
            // wanted simply by adding the handler to the given session
            // or link
            Session ssn = conn.session();

            // If a link doesn't have an event handler, the events go to
            // its parent session. If the session doesn't have a handler
            // the events go to its parent connection. If the connection
            // doesn't have a handler, the events go to the reactor.
            Sender snd = ssn.sender("sender");
            conn.open();
            ssn.open();
            snd.open();
        }

        @Override
        public void onLinkFlow(Event event) {
            Sender snd = (Sender)event.getLink();
            if (snd.getCredit() > 0) {
                byte[] msgData = new byte[1024];
                int length;
                while(true) {
                    try {
                        length = message.encode(msgData, 0, msgData.length);
                        break;
                    } catch(BufferOverflowException e) {
                        msgData = new byte[msgData.length * 2];
                    }
                }
                byte[] tag = String.valueOf(nextTag++).getBytes();
                Delivery dlv = snd.delivery(tag);
                snd.send(msgData, 0, length);
                dlv.settle();
                snd.advance();
                snd.close();
                snd.getSession().close();
                snd.getSession().getConnection().close();
            }
        }

        @Override
        public void onTransportError(Event event) {
            ErrorCondition condition = event.getTransport().getCondition();
            if (condition != null) {
                System.err.println("Error: " + condition.getDescription());
            } else {
                System.err.println("Error (no description returned).");
            }
        }
    }

    private final String host;
    private final int port;
    private final Message message;

    private Send(String host, int port, String content) {
        this.host = host;
        this.port = port;
        message = Proton.message();
        message.setBody(new AmqpValue(content));
    }

    @Override
    public void onReactorInit(Event event) {
        // You can use the connection method to create AMQP connections.

        // This connection's handler is the SendHandler object. All the events
        // for this connection will go to the SendHandler object instead of
        // going to the reactor. If you were to omit the SendHandler object,
        // all the events would go to the reactor.
        event.getReactor().connectionToHost(host, port, new SendHandler(message));
    }

    public static void main(String[] args) throws IOException {
        int port = 5672;
        String host = "localhost";
        if (args.length > 0) {
            String[] parts = args[0].split(":", 2);
            host = parts[0];
            if (parts.length > 1) {
                port = Integer.parseInt(parts[1]);
            }
        }
        String content = args.length > 1 ? args[1] : "Hello World!";

        Reactor r = Proton.reactor(new Send(host, port, content));
        r.run();
    }

}
