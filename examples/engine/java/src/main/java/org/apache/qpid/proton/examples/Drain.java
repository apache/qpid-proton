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

import java.util.ArrayList;
import java.util.List;

import org.apache.qpid.proton.engine.BaseHandler;
import org.apache.qpid.proton.engine.Collector;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Delivery;
import org.apache.qpid.proton.engine.Event;
import org.apache.qpid.proton.engine.Link;
import org.apache.qpid.proton.engine.Receiver;

public class Drain extends BaseHandler {

    private int count;
    private boolean block;
    private int received;
    private boolean quiet;

    public Drain(int count, boolean block, boolean quiet) {
        this.count = count;
        this.block = block;
        this.quiet = quiet;
    }

    @Override
    public void onLinkLocalOpen(Event evt) {
        Link link = evt.getLink();
        if (link instanceof Receiver) {
            Receiver receiver = (Receiver) link;

            if (block) {
                receiver.flow(count);
            } else {
                receiver.drain(count);
            }
        }
    }

    @Override
    public void onLinkFlow(Event evt) {
        Link link = evt.getLink();
        if (link instanceof Receiver) {
            Receiver receiver = (Receiver) link;

            if (!receiver.draining()) {
                receiver.getSession().getConnection().close();
            }
        }
    }

    @Override
    public void onDelivery(Event evt) {
        Delivery dlv = evt.getDelivery();
        if (dlv.getLink() instanceof Receiver) {
            Receiver receiver = (Receiver) dlv.getLink();

            if (!dlv.isPartial()) {
                byte[] bytes = new byte[dlv.pending()];
                receiver.recv(bytes, 0, bytes.length);
                Message msg = new Message(bytes);

                if (!quiet) {
                    System.out.println(String.format("Got message: %s", msg));
                }
                received++;
                dlv.settle();
            }

            if ((received >= count) || (!block && !receiver.draining())) {
                receiver.getSession().getConnection().close();
            }
        }
    }

    @Override
    public void onConnectionRemoteClose(Event evt) {
        System.out.println(String.format("Got %s messages", received));
    }

    public static void main(String[] argv) throws Exception {
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
        String address = args.isEmpty() || !args.get(0).startsWith("/") ? "//localhost" : args.remove(0);
        int count = args.isEmpty() ? 1 : Integer.parseInt(args.remove(0));
        boolean block = switches.contains("-b");

        Collector collector = Collector.Factory.create();

        Drain drain = new Drain(count, block, quiet);
        Driver driver = new Driver(collector, drain);

        Pool pool = new Pool(collector);
        pool.incoming(address, null);

        driver.run();
    }
}
