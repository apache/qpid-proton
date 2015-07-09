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

public class Counter extends BaseHandler {

    private long startTime;

    class CounterHandler extends BaseHandler {
        private final int limit;
        private int count;

        CounterHandler(int limit) {
            this.limit = limit;
        }

        @Override
        public void onTimerTask(Event event) {
            count += 1;
            System.out.println(count);
            if (count < limit) {
                // A recurring task can be accomplished by just scheduling
                // another event.
                event.getReactor().schedule(250, this);
            }
        }
    }

    @Override
    public void onReactorInit(Event event) {
        startTime = System.currentTimeMillis();
        System.out.println("Hello, World!");

        // Note that unlike the previous scheduling example, we pass in
        // a separate object for the handler. This means that the timer
        // event we just scheduled will not be seen by the Counter
        // implementation of BaseHandler as it is being handled by the
        // CounterHandler instance we create.
        event.getReactor().schedule(250, new CounterHandler(10));
    }

    @Override
    public void onReactorFinal(Event event) {
        long elapsedTime = System.currentTimeMillis() - startTime;
        System.out.println("Goodbye, World! (after " + elapsedTime + " long milliseconds)");
    }

    public static void main(String[] args) throws IOException {
        // In HelloWorld.java we said the reactor exits when there are no more
        // events to process. While this is true, it's not actually complete.
        // The reactor exits when there are no more events to process and no
        // possibility of future events arising. For that reason the reactor
        // will keep running until there are no more scheduled events and then
        // exit.
        Reactor reactor = Proton.reactor(new Counter());
        reactor.run();
    }
}
