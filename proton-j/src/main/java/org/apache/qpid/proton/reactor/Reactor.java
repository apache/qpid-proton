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

package org.apache.qpid.proton.reactor;

import java.io.IOException;
import java.util.Set;

import org.apache.qpid.proton.engine.Collector;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Handler;
import org.apache.qpid.proton.reactor.impl.ReactorImpl;



public interface Reactor {

    public static final class Factory
    {
        public static Reactor create() throws IOException {
            return new ReactorImpl();
        }
    }

    public long mark();
    public long now();
    public void attach(Object attachment);
    public Object attachment();
    public long getTimeout();

    public void setTimeout(long timeout);

    public Handler getGlobalHandler();

    public void setGlobalHandler(Handler handler);

    public Handler getHandler();

    public void setHandler(Handler handler);

/* TODO
 * pn_io_t *pn_reactor_io(pn_reactor_t *reactor) {
166   assert(reactor);
167   return reactor->io;
168 }
169

 */

    public Set<ReactorChild> children();

    public Collector collector();


    public Selectable selectable();



    public void update(Selectable selectable);


    void yield() ;

    public boolean quiesced();

    public boolean process();

    public void wakeup() throws IOException;

    public void start() ;

    public void stop() ;

    public void run();

    // pn_reactor_schedule from reactor.c
    public Task schedule(int delay, Handler handler);
    // TODO: acceptor
    // TODO: acceptorClose

    Connection connection(Handler handler);
}
