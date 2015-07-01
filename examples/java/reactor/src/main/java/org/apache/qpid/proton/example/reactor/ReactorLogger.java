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
import org.apache.qpid.proton.reactor.Reactor;

public class ReactorLogger extends BaseHandler {

    public static class Logger extends BaseHandler {
        @Override
        public void onUnhandled(Event event) {
            System.out.println("LOG: " + event);
        }
    }

    @Override
    public void onReactorInit(Event e) {
        System.out.println("Hello, World!");
    }

    @Override
    public void onReactorFinal(Event e) {
        System.out.println("Goodbye, World!");
    }

    private static boolean loggingEnabled = false;

    public static void main(String[] args) throws IOException {

        // You can pass multiple handlers to a reactor when you construct it.
        // Each of these handlers will see every event the reactor sees. By
        // combining this with on_unhandled, you can log each event that goes
        // to the reactor.
        Reactor reactor = Proton.reactor(new ReactorLogger(), new Logger());
        reactor.run();

        // Note that if you wanted to add the logger later, you could also
        // write the above as below. All arguments to the reactor are just
        // added to the default handler for the reactor.
        reactor = Proton.reactor(new ReactorLogger());
        if (loggingEnabled)
            reactor.getHandler().add(new Logger());
        reactor.run();
    }
}
