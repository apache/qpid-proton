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

package org.apache.qpid.proton.reactor.impl;

import java.util.concurrent.atomic.AtomicInteger;

import org.apache.qpid.proton.engine.Record;
import org.apache.qpid.proton.engine.impl.RecordImpl;
import org.apache.qpid.proton.reactor.Reactor;
import org.apache.qpid.proton.reactor.Task;

public class TaskImpl implements Task, Comparable<TaskImpl> {
    private final long deadline;
    private final int counter;
    private boolean cancelled = false;
    private final AtomicInteger count = new AtomicInteger();
    private Record attachments = new RecordImpl();
    private Reactor reactor;

    public TaskImpl(long deadline) {
        this.deadline = deadline;
        this.counter = count.getAndIncrement();
    }

    @Override
    public int compareTo(TaskImpl other) {
        int result;
        if (deadline < other.deadline) {
            result = -1;
        } else if (deadline > other.deadline) {
            result = 1;
        } else {
            result = counter - other.counter;
        }
        return result;
    }

    @Override
    public long deadline() {
        return deadline;
    }

    public boolean isCancelled() {
        return cancelled;
    }

    @Override
    public void cancel() {
        cancelled = true;
    }

    public void setReactor(Reactor reactor) {
        this.reactor = reactor;
    }

    @Override
    public Reactor getReactor() {
        return reactor;
    }

    @Override
    public Record attachments() {
        return attachments;
    }

}
