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
package org.apache.qpid.proton.examples;

import org.apache.qpid.proton.amqp.messaging.Accepted;
import org.apache.qpid.proton.engine.BaseHandler;
import org.apache.qpid.proton.engine.Collector;
import org.apache.qpid.proton.engine.Delivery;
import org.apache.qpid.proton.engine.Event;
import org.apache.qpid.proton.engine.Link;
import org.apache.qpid.proton.engine.Receiver;
import org.apache.qpid.proton.engine.Sender;

import java.io.IOException;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Deque;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Server
 *
 */

public class Server extends BaseHandler
{

    private class MessageStore {

        Map<String,Deque<Message>> messages = new HashMap<String,Deque<Message>>();

        void put(String address, Message message) {
            Deque<Message> queue = messages.get(address);
            if (queue == null) {
                queue = new ArrayDeque<Message>();
                messages.put(address, queue);
            }
            queue.add(message);
        }

        Message get(String address) {
            Deque<Message> queue = messages.get(address);
            if (queue == null) { return null; }
            Message msg = queue.remove();
            if (queue.isEmpty()) {
                messages.remove(address);
            }
            return msg;
        }

    }

    final private MessageStore messages = new MessageStore();
    final private Router router;
    private boolean quiet;
    private int tag = 0;

    public Server(Router router, boolean quiet) {
        this.router = router;
        this.quiet = quiet;
    }

    private byte[] nextTag() {
        return String.format("%s", tag++).getBytes();
    }

    private int send(String address) {
        return send(address, null);
    }

    private int send(String address, Sender snd) {
        if (snd == null) {
            Router.Routes<Sender> routes = router.getOutgoing(address);
            snd = routes.choose();
            if (snd == null) {
                return 0;
            }
        }

        int count = 0;
        while (snd.getCredit() > 0 && snd.getQueued() < 1024) {
            Message msg = messages.get(address);
            if (msg == null) {
                snd.drained();
                return count;
            }
            Delivery dlv = snd.delivery(nextTag());
            byte[] bytes = msg.getBytes();
            snd.send(bytes, 0, bytes.length);
            dlv.settle();
            count++;
            if (!quiet) {
                System.out.println(String.format("Sent message(%s): %s", address, msg));
            }
        }

        return count;
    }

    @Override
    public void onLinkFlow(Event evt) {
        Link link = evt.getLink();
        if (link instanceof Sender) {
            Sender snd = (Sender) link;
            send(router.getAddress(snd), snd);
        }
    }

    @Override
    public void onDelivery(Event evt) {
        Delivery dlv = evt.getDelivery();
        Link link = dlv.getLink();
        if (link instanceof Sender) {
            dlv.settle();
        } else {
            Receiver rcv = (Receiver) link;
            if (!dlv.isPartial()) {
                byte[] bytes = new byte[dlv.pending()];
                rcv.recv(bytes, 0, bytes.length);
                String address = router.getAddress(rcv);
                Message message = new Message(bytes);
                messages.put(address, message);
                dlv.disposition(Accepted.getInstance());
                dlv.settle();
                if (!quiet) {
                    System.out.println(String.format("Got message(%s): %s", address, message));
                }
                send(address);
            }
        }
    }

    public static final void main(String[] argv) throws IOException {
        List<String> switches = new ArrayList<String>();
        List<String> args = new ArrayList<String>();
        for (String s : argv) {
            if (s.startsWith("-")) {
                switches.add(s);
            } else {
                args.add(s);
            }
        }

        boolean quiet = switches.contains("-q");
        String host = !args.isEmpty() && !Character.isDigit(args.get(0).charAt(0)) ?
            args.remove(0) : "localhost";
        int port = !args.isEmpty() ? Integer.parseInt(args.remove(0)) : 5672;

        Collector collector = Collector.Factory.create();
        Router router = new Router();
        Driver driver = new Driver(collector, new Handshaker(),
                                   new FlowController(1024), router,
                                   new Server(router, quiet));
        driver.listen(host, port);
        driver.run();
    }

}
