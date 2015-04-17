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

package org.apache.qpid.proton.example.reactor;

import java.io.IOException;

import org.apache.qpid.proton.Proton;
import org.apache.qpid.proton.engine.BaseHandler;
import org.apache.qpid.proton.engine.Event;
import org.apache.qpid.proton.engine.Handler;
import org.apache.qpid.proton.reactor.Reactor;

// Events know how to dispatch themselves to handlers. By combining
// this with on_unhandled, you can provide a kind of inheritance
/// between handlers using delegation.
public class Delegates extends BaseHandler {

    private final Handler[] handlers;

    static class Hello extends BaseHandler {
        @Override
        public void onReactorInit(Event e) {
            System.out.println("Hello, World!");
        }
    }

    static class Goodbye extends BaseHandler {
        @Override
        public void onReactorFinal(Event e) {
            System.out.println("Goodbye, World!");
        }
    }

    public Delegates(Handler... handlers) {
        this.handlers = handlers;
    }

    @Override
    public void onUnhandled(Event event) {
        for (Handler handler : handlers) {
            event.dispatch(handler);
        }
    }

    public static void main(String[] args) throws IOException {
        Reactor reactor = Proton.reactor(new Delegates(new Hello(), new Goodbye()));
        reactor.run();
    }
}
