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

import java.util.PriorityQueue;

import org.apache.qpid.proton.engine.Collector;
import org.apache.qpid.proton.engine.Event.Type;
import org.apache.qpid.proton.engine.impl.CollectorImpl;
import org.apache.qpid.proton.reactor.Task;

public class Timer {

    private CollectorImpl collector;
    private PriorityQueue<TaskImpl> tasks = new PriorityQueue<TaskImpl>();

    public Timer(Collector collector) {
        this.collector = (CollectorImpl)collector;
    }

    Task schedule(long deadline) {
        TaskImpl task = new TaskImpl(deadline);
        tasks.add(task);
        return task;
    }

    long deadline() {
        flushCancelled();
        if (tasks.size() > 0) {
            Task task = tasks.peek();
            return task.deadline();
        } else {
            return 0;
        }
    }

    private void flushCancelled() {
        while (!tasks.isEmpty()) {
            TaskImpl task = tasks.peek();
            if (task.isCancelled())
                tasks.poll();
            else
                break;
        }
    }

    void tick(long now) {
        while(!tasks.isEmpty()) {
            TaskImpl task = tasks.peek();
            if (now >= task.deadline()) {
                tasks.poll();
                if (!task.isCancelled())
                    collector.put(Type.TIMER_TASK, task);
            } else {
                break;
            }
        }
    }

    int tasks() {
        flushCancelled();
        return tasks.size();
    }
}
