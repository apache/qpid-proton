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

import org.apache.qpid.proton.engine.Event.Type;
import org.apache.qpid.proton.engine.Extendable;
import org.apache.qpid.proton.engine.Handler;

/**
 * Represents work scheduled with a {@link Reactor} for execution at
 * some point in the future.
 * <p>
 * Tasks are created using the {@link Reactor#schedule(int, Handler)}
 * method.
 */
public interface Task extends Extendable {

    /**
     * @return the deadline at which the handler associated with the scheduled
     *         task should be delivered a {@link Type#TIMER_TASK} event.
     */
    long deadline();

    /** @return the reactor that created this task. */
    Reactor getReactor();

    /**
     * Cancel the execution of this task. No-op if invoked after the task was already executed.
     */
    void cancel();
}
