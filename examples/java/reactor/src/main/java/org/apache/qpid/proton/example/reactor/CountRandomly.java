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

// Let's try to modify our counter example. In addition to counting to
// 10 in quarter second intervals, let's also print out a random number
// every half second. This is not a super easy thing to express in a
// purely sequential program, but not so difficult using events.
public class CountRandomly extends BaseHandler {

    private long startTime;
    private CounterHandler counter;

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

            if (!done()) {
                event.getReactor().schedule(250, this);
            }
        }

        // Provide a method to check for doneness
        private boolean done() {
            return count >= limit;
        }
    }

    @Override
    public void onReactorInit(Event event) {
        startTime = System.currentTimeMillis();
        System.out.println("Hello, World!");

        // Save the counter instance in an attribute so we can refer to
        // it later.
        counter = new CounterHandler(10);
        event.getReactor().schedule(250, counter);

        // Now schedule another event with a different handler. Note
        // that the timer tasks go to separate handlers, and they don't
        // interfere with each other.
        event.getReactor().schedule(500, this);
    }

    @Override
    public void onTimerTask(Event event) {
        // keep on shouting until we are done counting
        System.out.println("Yay, " + Math.round(Math.abs((Math.random() * 110) - 10)));
        if (!counter.done()) {
            event.getReactor().schedule(500, this);
        }
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
        Reactor reactor = Proton.reactor(new CountRandomly());
        reactor.run();
    }
}
