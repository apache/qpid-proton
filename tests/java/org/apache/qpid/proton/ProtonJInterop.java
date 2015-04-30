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

package org.apache.qpid.proton;

import java.io.IOException;
import java.nio.BufferOverflowException;

import org.apache.qpid.proton.amqp.messaging.AmqpValue;
import org.apache.qpid.proton.amqp.transport.ErrorCondition;
import org.apache.qpid.proton.engine.BaseHandler;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Delivery;
import org.apache.qpid.proton.engine.EndpointState;
import org.apache.qpid.proton.engine.Event;
import org.apache.qpid.proton.engine.Sender;
import org.apache.qpid.proton.engine.Session;
import org.apache.qpid.proton.message.Message;
import org.apache.qpid.proton.reactor.Handshaker;
import org.apache.qpid.proton.reactor.Reactor;

public class ProtonJInterop {

    private static class SendHandler extends BaseHandler {

        private final String hostname;
        private int numMsgs;
        private int count = 0;
        private boolean result = false;

        private SendHandler(String hostname, int numMsgs) {
            this.hostname = hostname;
            this.numMsgs = numMsgs;
            add(new Handshaker());
        }

        @Override
        public void onConnectionInit(Event event) {
            Connection conn = event.getConnection();
            conn.setHostname(hostname);
            Session ssn = conn.session();
            Sender snd = ssn.sender("sender");
            conn.open();
            ssn.open();
            snd.open();
        }

        @Override
        public void onLinkFlow(Event event) {
            Sender snd = (Sender)event.getLink();
            if (snd.getCredit() > 0 && snd.getLocalState() != EndpointState.CLOSED) {
                Message message = Proton.message();
                ++count;
                message.setBody(new AmqpValue("message-"+count));
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
                byte[] tag = String.valueOf(count).getBytes();
                Delivery dlv = snd.delivery(tag);
                snd.send(msgData, 0, length);
                dlv.settle();
                snd.advance();
                if (count == numMsgs) {
                    snd.close();
                    snd.getSession().close();
                    snd.getSession().getConnection().close();
                    result = true;
                }
            }
        }

        @Override
        public void onTransportError(Event event) {
            result = false;
            ErrorCondition condition = event.getTransport().getCondition();
            if (condition != null) {
                System.err.println("Error: " + condition.getDescription());
            } else {
                System.err.println("Error (no description returned).");
            }
        }
    }

    private static class Send extends BaseHandler {
        private final SendHandler sendHandler;

        private Send(String hostname, int numMsgs) {
            sendHandler = new SendHandler(hostname, numMsgs);
        }

        @Override
        public void onReactorInit(Event event) {
            event.getReactor().connection(sendHandler);
        }

        public boolean getResult() {
            return sendHandler.result;
        }
    }

    private static boolean sendTest(String[] args) throws IOException {
        int port = Integer.valueOf(args[0]);
        int numMsgs = Integer.valueOf(args[1]);
        Send send = new Send("localhost:" + port, numMsgs);
        Reactor r = Proton.reactor(send);
        r.run();
        return send.getResult();
    }

    public static void main(String[] args) throws IOException {
        try {
            System.exit(sendTest(args) ? 0 : 1);
        } catch(Throwable t) {
            t.printStackTrace();
            System.exit(1);
        }
    }
}