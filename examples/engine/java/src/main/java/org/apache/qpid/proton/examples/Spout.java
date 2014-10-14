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
import org.apache.qpid.proton.engine.Sender;

public class Spout extends BaseHandler
{
    private int count;
    private int sent;
    private int settled;
    private boolean quiet;

    public Spout(int count, boolean quiet) {
        this.count = count;
        this.quiet = quiet;
    }

    @Override
    public void onLinkFlow(Event evt) {
        Link link = evt.getLink();
        if (link instanceof Sender) {
            Sender sender = (Sender) link;
            while ((sent < count) && sender.getCredit() > 0) {
                Delivery dlv = sender.delivery(String.format("spout-%s", sent).getBytes());

                Message msg = new Message(String.format("Hello World! [%s]", sent));
                byte[] bytes = msg.getBytes();
                sender.send(bytes, 0, bytes.length);
                sender.advance();

                if (!quiet) {
                    System.out.println(String.format("Sent %s to %s: %s", new String(dlv.getTag()),
                                                     sender.getTarget().getAddress(), msg));
                }
                sent++;
            }
        }
    }

    @Override
    public void onDelivery(Event evt) {
        Delivery dlv = evt.getDelivery();
        if (dlv.remotelySettled()) {
            if (!quiet) {
                System.out.println(String.format("Settled %s: %s", new String(dlv.getTag()), dlv.getRemoteState()));
            }
            dlv.settle();
            settled++;
        }

        if (settled >= count) {
            dlv.getLink().getSession().getConnection().close();
        }
    }

    @Override
    public void onConnectionRemoteClose(Event evt) {
        System.out.println("settled: " + settled);
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
        String address = !args.isEmpty() && args.get(0).startsWith("/") ?
            args.remove(0) : "//localhost";
        int count = !args.isEmpty() ? Integer.parseInt(args.remove(0)) : 1;

        Collector collector = Collector.Factory.create();

        Spout spout = new Spout(count, quiet);

        Driver driver = new Driver(collector, spout);

        Pool pool = new Pool(collector);
        pool.outgoing(address, null);

        driver.run();
    }
}
