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

import org.apache.qpid.proton.engine.Collector;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Event;
import org.apache.qpid.proton.engine.Session;
import org.apache.qpid.proton.engine.Link;
import org.apache.qpid.proton.engine.Sender;
import org.apache.qpid.proton.engine.Receiver;

import org.apache.qpid.proton.amqp.messaging.Source;
import org.apache.qpid.proton.amqp.messaging.Target;

import java.util.HashMap;
import java.util.Map;

/**
 * Pool
 *
 */

public class Pool
{

    final private Collector collector;
    final private Map<String,Connection> connections;

    final private LinkConstructor<Sender> outgoingConstructor = new LinkConstructor<Sender> () {
        public Sender create(Session ssn, String remote, String local) {
            return newOutgoing(ssn, remote, local);
        }
    };
    final private LinkConstructor<Receiver> incomingConstructor = new LinkConstructor<Receiver> () {
        public Receiver create(Session ssn, String remote, String local) {
            return newIncoming(ssn, remote, local);
        }
    };

    final private LinkResolver<Sender> outgoingResolver;
    final private LinkResolver<Receiver> incomingResolver;

    public Pool(Collector collector, final Router router) {
        this.collector = collector;
        connections = new HashMap<String,Connection>();

        if (router != null) {
            outgoingResolver = new LinkResolver<Sender>() {
                public Sender resolve(String address) {
                    return router.getOutgoing(address).choose();
                }
            };
            incomingResolver = new LinkResolver<Receiver>() {
                public Receiver resolve(String address) {
                    return router.getIncoming(address).choose();
                }
            };
        } else {
            outgoingResolver = new LinkResolver<Sender>() {
                public Sender resolve(String address) { return null; }
            };
            incomingResolver = new LinkResolver<Receiver>() {
                public Receiver resolve(String address) { return null; }
            };
        }
    }

    public Pool(Collector collector) {
        this(collector, null);
    }

    private <T extends Link> T resolve(String remote, String local,
                                       LinkResolver<T> resolver,
                                       LinkConstructor<T> constructor) {
        String host = remote.substring(2).split("/", 2)[0];
        T link = resolver.resolve(remote);
        if (link == null) {
            Connection conn = connections.get(host);
            if (conn == null) {
                conn = Connection.Factory.create();
                conn.collect(collector);
                conn.setHostname(host);
                conn.open();
                connections.put(host, conn);
            }

            Session ssn = conn.session();
            ssn.open();

            link = constructor.create(ssn, remote, local);
            link.open();
        }
        return link;
    }

    public Sender outgoing(String target, String source) {
        return resolve(target, source, outgoingResolver, outgoingConstructor);
    }

    public Receiver incoming(String source, String target) {
        return resolve(source, target, incomingResolver, incomingConstructor);
    }

    public Sender newOutgoing(Session ssn, String remote, String local) {
        Sender snd = ssn.sender(String.format("%s-%s", local, remote));
        Source src = new Source();
        src.setAddress(local);
        snd.setSource(src);
        Target tgt = new Target();
        tgt.setAddress(remote);
        snd.setTarget(tgt);
        return snd;
    }

    public Receiver newIncoming(Session ssn, String remote, String local) {
        Receiver rcv = ssn.receiver(String.format("%s-%s", remote, local));
        Source src = new Source();
        src.setAddress(remote);
        rcv.setSource(src);
        Target tgt = new Target();
        tgt.setAddress(remote);
        rcv.setTarget(tgt);
        return rcv;
    }

    public static interface LinkConstructor<T extends Link> {
        T create(Session session, String remote, String local);
    }

    public static interface LinkResolver<T extends Link> {
        T resolve(String remote);
    }

}
