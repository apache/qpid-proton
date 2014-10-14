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

import org.apache.qpid.proton.amqp.transport.Source;
import org.apache.qpid.proton.amqp.transport.Target;
import org.apache.qpid.proton.engine.BaseHandler;
import org.apache.qpid.proton.engine.Event;
import org.apache.qpid.proton.engine.Link;
import org.apache.qpid.proton.engine.Receiver;
import org.apache.qpid.proton.engine.Sender;


import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ThreadLocalRandom;


/**
 * Router
 *
 */

public class Router extends BaseHandler
{

    public static class Routes<T extends Link> {

        List<T> routes = new ArrayList<T>();

        void add(T route) {
            routes.add(route);
        }

        void remove(T route) {
            routes.remove(route);
        }

        int size() {
            return routes.size();
        }

        public T choose() {
            if (routes.isEmpty()) { return null; }
            ThreadLocalRandom rand = ThreadLocalRandom.current();
            int idx = rand.nextInt(0, routes.size());
            return routes.get(idx);
        }

    }

    private static final Routes<Sender> EMPTY_OUT = new Routes<Sender>();
    private static final Routes<Receiver> EMPTY_IN = new Routes<Receiver>();

    final private Map<String,Routes<Sender>> outgoing = new HashMap<String,Routes<Sender>>();
    final private Map<String,Routes<Receiver>> incoming = new HashMap<String,Routes<Receiver>>();

    public Router() {}

    private String getAddress(Source source) {
        if (source == null) {
            return null;
        } else {
            return source.getAddress();
        }
    }

    private String getAddress(Target target) {
        if (target == null) {
            return null;
        } else {
            return target.getAddress();
        }
    }

    public String getAddress(Sender snd) {
        String source = getAddress(snd.getSource());
        String target = getAddress(snd.getTarget());
        return source != null ? source : target;
    }

    public String getAddress(Receiver rcv) {
        return getAddress(rcv.getTarget());
    }

    public Routes<Sender> getOutgoing(String address) {
        Routes<Sender> routes = outgoing.get(address);
        if (routes == null) { return EMPTY_OUT; }
        return routes;
    }

    public Routes<Receiver> getIncoming(String address) {
        Routes<Receiver> routes = incoming.get(address);
        if (routes == null) { return EMPTY_IN; }
        return routes;
    }

    private void add(Sender snd) {
        String address = getAddress(snd);
        Routes<Sender> routes = outgoing.get(address);
        if (routes == null) {
            routes = new Routes<Sender>();
            outgoing.put(address, routes);
        }
        routes.add(snd);
    }

    private void remove(Sender snd) {
        String address = getAddress(snd);
        Routes<Sender> routes = outgoing.get(address);
        if (routes != null) {
            routes.remove(snd);
            if (routes.size() == 0) {
                outgoing.remove(address);
            }
        }
    }

    private void add(Receiver rcv) {
        String address = getAddress(rcv);
        Routes<Receiver> routes = incoming.get(address);
        if (routes == null) {
            routes = new Routes<Receiver>();
            incoming.put(address, routes);
        }
        routes.add(rcv);
    }

    private void remove(Receiver rcv) {
        String address = getAddress(rcv);
        Routes<Receiver> routes = incoming.get(address);
        if (routes != null) {
            routes.remove(rcv);
            if (routes.size() == 0) {
                incoming.remove(address);
            }
        }
    }

    private void add(Link link) {
        if (link instanceof Sender) {
            add((Sender) link);
        } else {
            add((Receiver) link);
        }
    }

    private void remove(Link link) {
        if (link instanceof Sender) {
            remove((Sender) link);
        } else {
            remove((Receiver) link);
        }
    }

    @Override
    public void onLinkLocalOpen(Event evt) {
        add(evt.getLink());
    }

    @Override
    public void onLinkLocalClose(Event evt) {
        remove(evt.getLink());
    }

    @Override
    public void onLinkFinal(Event evt) {
        remove(evt.getLink());
    }

}
