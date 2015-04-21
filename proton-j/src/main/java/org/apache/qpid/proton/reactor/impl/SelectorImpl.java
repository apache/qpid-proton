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

import java.io.IOException;
import java.nio.channels.SelectionKey;
import java.util.HashSet;
import java.util.Iterator;

import org.apache.qpid.proton.reactor.Selectable;
import org.apache.qpid.proton.reactor.Selector;

public class SelectorImpl implements Selector {

    private final java.nio.channels.Selector selector;
    private final HashSet<Selectable> selectables = new HashSet<Selectable>();
    private final HashSet<Selectable> readable = new HashSet<Selectable>();
    private final HashSet<Selectable> writeable = new HashSet<Selectable>();
    private final HashSet<Selectable> expired = new HashSet<Selectable>();
    private final HashSet<Selectable> error = new HashSet<Selectable>();

    public SelectorImpl() throws IOException {
        selector = java.nio.channels.Selector.open();
    }

    @Override
    public void add(Selectable selectable) throws IOException {
        // TODO: valid for selectable to have a 'null' channel - in this case it can only expire...
        if (selectable.getChannel() != null) {
            selectable.getChannel().configureBlocking(false);
            SelectionKey key = selectable.getChannel().register(selector, 0);
            key.attach(selectable);
        }
        selectables.add(selectable);
        update(selectable);
    }

    @Override
    public void update(Selectable selectable) {
        if (selectable.getChannel() != null) {
            int interestedOps = 0;
            if (selectable.isReading()) interestedOps |= SelectionKey.OP_READ;
            if (selectable.isWriting()) interestedOps |= SelectionKey.OP_WRITE;
            SelectionKey key = selectable.getChannel().keyFor(selector);
            key.interestOps(interestedOps);
        }
    }

    @Override
    public void remove(Selectable selectable) {
        if (selectable.getChannel() != null) {
            SelectionKey key = selectable.getChannel().keyFor(selector);
            key.cancel();
            key.attach(null);
        }
        selectables.remove(selectable);
    }

    @Override
    public void select(long timeout) throws IOException {
        if (timeout > 0) {
            long deadline = 0;
            for (Selectable selectable : selectables) {    // TODO: this differs from the C code which requires a call to update() to make deadline changes take affect
                long d = selectable.getDeadline();
                if (d > 0) {
                    deadline = (deadline == 0) ? d : Math.min(deadline,  d);
                }
            }

            if (deadline > 0) {
                long now = System.currentTimeMillis();
                long delta = deadline - now;
                if (delta < 0) {
                    timeout = 0;
                } else if (delta < timeout) {
                    timeout = delta;
                }
            }
        }

        if (timeout > 0) {
            selector.select(timeout);
        } else {
            selector.selectNow();
        }
        long awoken = System.currentTimeMillis();

        readable.clear();
        writeable.clear();
        expired.clear();
        error.clear();  // TODO: nothing ever gets put in here...
        for (SelectionKey key : selector.selectedKeys()) {
            Selectable selectable = (Selectable)key.attachment();
            if (key.isReadable()) readable.add(selectable);
            if (key.isWritable()) writeable.add(selectable);
        }
        selector.selectedKeys().clear();
        for (Selectable selectable : selectables) {    // TODO: this is different to the C code which evaluates expiry at the point the selectable is iterated over.
            long deadline = selectable.getDeadline();
            if (deadline > 0 && awoken >= deadline) {
                expired.add(selectable);
            }
        }
    }

    @Override
    public Iterator<Selectable> readable() {
        return readable.iterator();
    }

    @Override
    public Iterator<Selectable> writeable() {
        return writeable.iterator();
    }

    @Override
    public Iterator<Selectable> expired() {
        return expired.iterator();
    }

    @Override
    public Iterator<Selectable> error() {
        return error.iterator();
    }

}
