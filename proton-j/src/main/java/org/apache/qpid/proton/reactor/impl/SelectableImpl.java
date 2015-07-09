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

import java.nio.channels.SelectableChannel;

import org.apache.qpid.proton.engine.Collector;
import org.apache.qpid.proton.engine.Event.Type;
import org.apache.qpid.proton.engine.Record;
import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.engine.impl.CollectorImpl;
import org.apache.qpid.proton.engine.impl.RecordImpl;
import org.apache.qpid.proton.reactor.Reactor;
import org.apache.qpid.proton.reactor.Selectable;

public class SelectableImpl implements Selectable {

    private Callback readable;
    private Callback writable;
    private Callback error;
    private Callback expire;
    private Callback release;
    private Callback free;

    private boolean reading = false;
    private boolean writing = false;
    private long deadline = 0;
    private SelectableChannel channel;
    private Record attachments = new RecordImpl();
    private boolean registered;
    private Reactor reactor;
    private Transport transport;
    private boolean terminal;
    private boolean terminated;

    @Override
    public boolean isReading() {
        return reading;
    }

    @Override
    public boolean isWriting() {
        return writing;
    }

    @Override
    public long getDeadline() {
        return deadline;
    }

    @Override
    public void setReading(boolean reading) {
        this.reading = reading;
    }

    @Override
    public void setWriting(boolean writing) {
        this.writing = writing;
    }

    @Override
    public void setDeadline(long deadline) {
        this.deadline = deadline;
    }

    @Override
    public void onReadable(Callback runnable) {
        this.readable = runnable;
    }

    @Override
    public void onWritable(Callback runnable) {
        this.writable = runnable;
    }

    @Override
    public void onExpired(Callback runnable) {
        this.expire = runnable;
    }

    @Override
    public void onError(Callback runnable) {
        this.error = runnable;
    }

    @Override
    public void onRelease(Callback runnable) {
        this.release = runnable;
    }

    @Override
    public void onFree(Callback runnable) {
        this.free = runnable;
    }

    @Override
    public void readable() {
        if (readable != null) {
            readable.run(this);
        }
    }

    @Override
    public void writeable() {
        if (writable != null) {
            writable.run(this);
        }
    }

    @Override
    public void expired() {
        if (expire != null) {
            expire.run(this);
        }
    }

    @Override
    public void error() {
        if (error != null) {
            error.run(this);
        }
    }

    @Override
    public void release() {
        if (release != null) {
            release.run(this);
        }
    }

    @Override
    public void free() {
        if (free != null) {
            free.run(this);
        }
    }

    @Override
    public void setChannel(SelectableChannel channel) {
        this.channel = channel;
    }

    @Override
    public SelectableChannel getChannel() {
        return channel;
    }

    @Override
    public boolean isRegistered() {
        return registered;
    }

    @Override
    public void setRegistered(boolean registered) {
        this.registered = registered;
    }

    @Override
    public void setCollector(final Collector collector) {
        final CollectorImpl collectorImpl = (CollectorImpl)collector;

        onReadable(new Callback() {
            @Override
            public void run(Selectable selectable) {
                collectorImpl.put(Type.SELECTABLE_READABLE, selectable);
            }
        });
        onWritable(new Callback() {
            @Override
            public void run(Selectable selectable) {
                collectorImpl.put(Type.SELECTABLE_WRITABLE, selectable);
            }
        });
        onExpired(new Callback() {
            @Override
            public void run(Selectable selectable) {
                collectorImpl.put(Type.SELECTABLE_EXPIRED, selectable);
            }
        });
        onError(new Callback() {
            @Override
            public void run(Selectable selectable) {
                collectorImpl.put(Type.SELECTABLE_ERROR, selectable);
            }
        });
    }

    @Override
    public Reactor getReactor() {
        return reactor;
    }

    @Override
    public void terminate() {
        terminal = true;
    }

    @Override
    public boolean isTerminal() {
        return terminal;
    }

    protected Transport getTransport() {
        return transport;
    }

    protected void setTransport(Transport transport) {
        this.transport = transport;
    }

    protected void setReactor(Reactor reactor) {
        this.reactor = reactor;
    }

    @Override
    public Record attachments() {
        return attachments;
    }

    public boolean isTerminated() {
        return terminated;
    }

    public void terminated() {
        terminated = true;
    }
}
