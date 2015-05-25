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
import org.apache.qpid.proton.engine.Record;
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
    public Record attachments();
    public long getTimeout();

    public void setTimeout(long timeout);

    public Handler getGlobalHandler();

    public void setGlobalHandler(Handler handler);

    public Handler getHandler();

    public void setHandler(Handler handler);

    public Set<ReactorChild> children();

    public Collector collector();


    public Selectable selectable();



    public void update(Selectable selectable);


    void yield() ;

    public boolean quiesced();

    public boolean process() throws HandlerException;

    public void wakeup() throws IOException;

    public void start() ;

    public void stop() throws HandlerException;

    public void run() throws HandlerException;

    // pn_reactor_schedule from reactor.c
    public Task schedule(int delay, Handler handler);

    Connection connection(Handler handler);

    Acceptor acceptor(String host, int port) throws IOException;
    Acceptor acceptor(String host, int port, Handler handler) throws IOException;

    // This also frees any children that the reactor has!
    public void free();
}
