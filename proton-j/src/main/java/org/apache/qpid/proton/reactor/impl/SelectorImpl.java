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
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.util.HashSet;
import java.util.Iterator;

import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.transport.ErrorCondition;
import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.reactor.Selectable;
import org.apache.qpid.proton.reactor.Selector;

class SelectorImpl implements Selector {

    private final java.nio.channels.Selector selector;
    private final HashSet<Selectable> selectables = new HashSet<Selectable>();
    private final HashSet<Selectable> readable = new HashSet<Selectable>();
    private final HashSet<Selectable> writeable = new HashSet<Selectable>();
    private final HashSet<Selectable> expired = new HashSet<Selectable>();
    private final HashSet<Selectable> error = new HashSet<Selectable>();

    protected SelectorImpl(IO io) throws IOException {
        selector = io.selector();
    }

    @Override
    public void add(Selectable selectable) throws IOException {
        // Selectable can be 'null' - if this is the case it can only ever receive expiry events.
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
            if (selectable.getChannel() instanceof SocketChannel &&
                    ((SocketChannel)selectable.getChannel()).isConnectionPending()) {
                interestedOps |= SelectionKey.OP_CONNECT;
            } else {
                if (selectable.isReading()) {
                    if (selectable.getChannel() instanceof ServerSocketChannel) {
                        interestedOps |= SelectionKey.OP_ACCEPT;
                    } else {
                        interestedOps |= SelectionKey.OP_READ;
                    }
                }
                if (selectable.isWriting()) interestedOps |= SelectionKey.OP_WRITE;
            }
            SelectionKey key = selectable.getChannel().keyFor(selector);
            key.interestOps(interestedOps);
        }
    }

    @Override
    public void remove(Selectable selectable) {
        if (selectable.getChannel() != null) {
            SelectionKey key = selectable.getChannel().keyFor(selector);
            if (key != null) {
                key.cancel();
                key.attach(null);
            }
        }
        selectables.remove(selectable);
    }

    @Override
    public void select(long timeout) throws IOException {

        long now = System.currentTimeMillis();
        if (timeout > 0) {
            long deadline = 0;
            // XXX: Note: this differs from the C code which requires a call to update() to make deadline changes take affect
            for (Selectable selectable : selectables) {
                long d = selectable.getDeadline();
                if (d > 0) {
                    deadline = (deadline == 0) ? d : Math.min(deadline,  d);
                }
            }

            if (deadline > 0) {
                long delta = deadline - now;
                if (delta < 0) {
                    timeout = 0;
                } else if (delta < timeout) {
                    timeout = delta;
                }
            }
        }

        error.clear();

        long awoken = 0;
        if (timeout > 0) {
            long remainingTimeout = timeout;
            while(remainingTimeout > 0) {
                selector.select(remainingTimeout);
                awoken = System.currentTimeMillis();

                for (Iterator<SelectionKey> iterator = selector.selectedKeys().iterator(); iterator.hasNext();) {
                    SelectionKey key = iterator.next();
                    if (key.isConnectable()) {
                        try {
                            ((SocketChannel)key.channel()).finishConnect();
                            update((Selectable)key.attachment());
                        } catch(IOException ioException) {
                            SelectableImpl selectable = (SelectableImpl)key.attachment();
                            ErrorCondition condition = new ErrorCondition();
                            condition.setCondition(Symbol.getSymbol("proton:io"));
                            condition.setDescription(ioException.getMessage());
                            Transport transport = selectable.getTransport();
                            if (transport != null) {
                                transport.setCondition(condition);
                                transport.close_tail();
                                transport.close_head();
                                transport.pop(Math.max(0, transport.pending())); // Force generation of TRANSPORT_HEAD_CLOSE (not in C code)
                            }
                            error.add(selectable);
                        }
                        iterator.remove();
                    }
                }
                if (!selector.selectedKeys().isEmpty()) {
                    break;
                }
                remainingTimeout = remainingTimeout - (awoken - now);
            }
        } else {
            selector.selectNow();
            awoken = System.currentTimeMillis();
        }

        readable.clear();
        writeable.clear();
        expired.clear();
        for (SelectionKey key : selector.selectedKeys()) {
            Selectable selectable = (Selectable)key.attachment();
            if (key.isReadable()) readable.add(selectable);
            if (key.isAcceptable()) readable.add(selectable);
            if (key.isWritable()) writeable.add(selectable);
        }
        selector.selectedKeys().clear();
        // XXX: Note: this is different to the C code which evaluates expiry at the point the selectable is iterated over.
        for (Selectable selectable : selectables) {
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

    @Override
    public void free() {
        try {
            selector.close();
        } catch(IOException ioException) {
            // Ignore
        }
    }
}
