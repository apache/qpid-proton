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

import org.apache.qpid.proton.amqp.messaging.AmqpValue;
import org.apache.qpid.proton.message.Message;
import org.apache.qpid.proton.message.impl.MessageImpl;
import org.apache.qpid.proton.messenger.Messenger;
import org.apache.qpid.proton.messenger.impl.MessengerImpl;

import java.io.IOException;
import java.util.Arrays;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 * Example/test of the java Messenger/Message API.
 * Based closely qpid src/proton/examples/messenger/py/send.py
 * @author mberkowitz@sf.org
 * @since 8/4/2013
 */
public class Send {

    private static Logger tracer = Logger.getLogger("proton.example");
    private String address = "amqp://0.0.0.0";
    private String subject;
    private String[] bodies = new String[]{"Hello World!"};

    private static void usage() {
        System.err.println("Usage: send [-a ADDRESS] [-s SUBJECT] MSG+");
        System.exit(2);
    }

    private Send(String args[]) {
        int i = 0;
        while (i < args.length) {
            String arg = args[i++];
            if (arg.startsWith("-")) {
                if ("-a".equals(arg)) {
                    address = args[i++];
                } else if ("-s".equals(arg)) {
                    subject = args[i++];
                } else {
                    System.err.println("unknown option " + arg);
                    usage();
                }
            } else {
                --i;
                break;
            }
        }

        if(i != args.length)
        {
            bodies = Arrays.copyOfRange(args, i, args.length);
        }
    }

    private void run() {
        try {
            Messenger mng = new MessengerImpl();
            mng.start();
            Message msg = new MessageImpl();
            msg.setAddress(address);
            if (subject != null) msg.setSubject(subject);
            for (String body : bodies) {
                msg.setBody(new AmqpValue(body));
                mng.put(msg);
            }
            mng.send();
            mng.stop();
        } catch (Exception e) {
            tracer.log(Level.SEVERE, "proton error", e);
        }
    }

    public static void main(String args[]) {
        Send o = new Send(args);
        o.run();
    }
}



