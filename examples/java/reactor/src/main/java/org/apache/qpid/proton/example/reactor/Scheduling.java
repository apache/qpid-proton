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
import org.apache.qpid.proton.reactor.Task;

public class Scheduling extends BaseHandler {

    private long startTime;

    @Override
    public void onReactorInit(Event event) {
        startTime = System.currentTimeMillis();
        System.out.println("Hello, World!");
        Task task = event.getReactor().schedule(1000, this);
        task.setAttachment("Yay");
    }

    @Override
    public void onTimerTask(Event event) {
        Task task = event.getTask();
        System.out.println(task.getAttachment() + " my task is complete!");
    }

    @Override
    public void onReactorFinal(Event e) {
        long elapsedTime = System.currentTimeMillis() - startTime;
        System.out.println("Goodbye, World! (after " + elapsedTime + " long milliseconds)");
    }

    public static void main(String[] args) throws IOException {
        Reactor reactor = Proton.reactor(new Scheduling());
        reactor.run();
    }
}
