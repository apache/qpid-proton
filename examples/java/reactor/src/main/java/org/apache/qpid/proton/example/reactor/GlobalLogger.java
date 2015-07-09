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

// Not every event goes to the reactor's event handler. If we have a
// separate handler for something like a scheduled task, then those
// events aren't logged by the logger associated with the reactor's
// handler. Sometimes this is useful if you don't want to see them, but
// sometimes you want the global picture.
public class GlobalLogger extends BaseHandler {

    static class Logger extends BaseHandler {
        @Override
        public void onUnhandled(Event event) {
            System.out.println("LOG: " + event);
        }
    }

    static class Task extends BaseHandler {
        @Override
        public void onTimerTask(Event e) {
            System.out.println("Mission accomplished!");
        }
    }

    @Override
    public void onReactorInit(Event event) {
        System.out.println("Hello, World!");
        event.getReactor().schedule(0, new Task());
    }

    @Override
    public void onReactorFinal(Event e) {
        System.out.println("Goodbye, World!");
    }

    public static void main(String[] args) throws IOException {
        Reactor reactor = Proton.reactor(new GlobalLogger());

        // In addition to having a regular handler, the reactor also has a
        // global handler that sees every event. By adding the Logger to the
        // global handler instead of the regular handler, we can log every
        // single event that occurs in the system regardless of whether or not
        // there are specific handlers associated with the objects that are the
        // target of those events.
        reactor.getGlobalHandler().add(new Logger());
        reactor.run();
    }
}
