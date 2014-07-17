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
package org.apache.qpid.proton.engine.impl;

import org.apache.qpid.proton.engine.Collector;
import org.apache.qpid.proton.engine.Event;

import java.util.LinkedList;
import java.util.Queue;


/**
 * CollectorImpl
 *
 */

public class CollectorImpl implements Collector
{

    private EventImpl head;
    private EventImpl tail;
    private EventImpl free;

    public CollectorImpl()
    {}

    public Event peek()
    {
        return head;
    }

    public void pop()
    {
        if (head != null) {
            EventImpl next = head.next;
            head.next = free;
            free = head;
            head.clear();
            head = next;
        }
    }

    public EventImpl put(Event.Type type, Object context)
    {
        if (tail != null && tail.getType() == type &&
            tail.getContext() == context) {
            return null;
        }

        EventImpl event;
        if (free == null) {
            event = new EventImpl();
        } else {
            event = free;
            free = free.next;
            event.next = null;
        }

        event.init(type, context);

        if (head == null) {
            head = event;
            tail = event;
        } else {
            tail.next = event;
            tail = event;
        }

        return event;
    }

}
