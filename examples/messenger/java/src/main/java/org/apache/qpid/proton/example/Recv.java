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
package org.apache.qpid.proton.example;

import org.apache.qpid.proton.message.Message;
import org.apache.qpid.proton.messenger.Messenger;
import org.apache.qpid.proton.messenger.impl.MessengerImpl;
import org.apache.qpid.proton.amqp.messaging.ApplicationProperties;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 * Example/test of the java Messenger/Message API.
 * Based closely qpid src/proton/examples/messenger/py/recv.py
 * @author mberkowitz@sf.org
 * @since 8/4/2013
 */
public class Recv {
    private static Logger tracer = Logger.getLogger("proton.example");
    private boolean verbose = false;
    private int maxct = 0;
    private List<String> addrs = new ArrayList<String>();

    private static void usage() {
        System.err.println("Usage: recv [-v] [-n MAXCT] [-a ADDRESS]*");
        System.exit(2);
    }

    private Recv(String args[]) {
        int i = 0;
        while (i < args.length) {
            String arg = args[i++];
            if (arg.startsWith("-")) {
                if ("-v".equals(arg)) {
                    verbose = true;
                } else if ("-a".equals(arg)) {
                    addrs.add(args[i++]);
                } else if ("-n".equals(arg)) {
                    maxct = Integer.valueOf(args[i++]);
                } else {
                    System.err.println("unknown option " + arg);
                    usage();
                }
            } else {
                usage();
            }
        }
        if (addrs.size() == 0) {
            addrs.add("amqp://~0.0.0.0");
        }
    }

    private static String safe(Object o) {
        return String.valueOf(o);
    }

    private void print(int i, Message msg) {
        StringBuilder b = new StringBuilder("message: ");
        b.append(i).append("\n");
        b.append("Address: ").append(msg.getAddress()).append("\n");
        b.append("Subject: ").append(msg.getSubject()).append("\n");
        if (verbose) {
            b.append("Props:     ").append(msg.getProperties()).append("\n");
            b.append("App Props: ").append(msg.getApplicationProperties()).append("\n");
            b.append("Msg Anno:  ").append(msg.getMessageAnnotations()).append("\n");
            b.append("Del Anno:  ").append(msg.getDeliveryAnnotations()).append("\n");
        } else {
            ApplicationProperties p = msg.getApplicationProperties();
            String s = (p == null) ? "null" : safe(p.getValue());
            b.append("Headers: ").append(s).append("\n");
        }
        b.append(msg.getBody()).append("\n");
        b.append("END").append("\n");
        System.out.println(b.toString());
    }

    private void run() {
        try {
            Messenger mng = new MessengerImpl();
            mng.start();
            for (String a : addrs) {
                mng.subscribe(a);
            }
            int ct = 0;
            boolean done = false;
            while (!done) {
                mng.recv();
                while (mng.incoming() > 0) {
                    Message msg = mng.get();
                    ++ct;
                    print(ct, msg);
                    if (maxct > 0 && ct >= maxct) {
                        done = true;
                        break;
                    }
                }
            }
            mng.stop();
        } catch (Exception e) {
            tracer.log(Level.SEVERE, "proton error", e);
        }
    }

    public static void main(String args[]) {
        Recv o = new Recv(args);
        o.run();
    }
}

