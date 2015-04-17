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
import java.util.HashMap;

import org.apache.qpid.proton.engine.BaseHandler;
import org.apache.qpid.proton.engine.Collector;
import org.apache.qpid.proton.engine.Event.Type;
import org.apache.qpid.proton.engine.Handler;
import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.engine.impl.CollectorImpl;
import org.apache.qpid.proton.reactor.Reactor;
import org.apache.qpid.proton.reactor.Selectable;

public class SelectableImpl implements Selectable {

    private Callback readable;
    private Callback writable;
    private Callback error;
    private Callback expire;
    private Callback release;
    private Callback finalize;

    private boolean reading = false;
    private boolean writing = false;
    private long deadline = 0;
    private SelectableChannel channel;
    private Object attachment;
    private boolean registered;
    private Reactor reactor;
    private Transport transport;
    private boolean terminal;

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
    public void onFinalize(Callback runnable) {
        this.finalize = runnable;
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
    public void _finalize() {
        if (finalize != null) {
            finalize.run(this);
        }
    }

    // These are equivalent to the C code's set/get file descritor functions.
    @Override
    public void setChannel(SelectableChannel channel) {
        this.channel = channel;
    }

    @Override
    public SelectableChannel getChannel() {
        return channel;
    }

    @Override
    public void setAttachment(Object attachment) {
        this.attachment = attachment;
    }

    @Override
    public Object getAttachment() {
        return attachment;
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
    public Reactor getReactor() {  // TODO: the C version uses set/getContext for this - should we do the same?
        return reactor;
    }

    @Override
    public void terminate() {
        terminal = true;
    }

    private final HashMap<RecordKeyType, RecordValueType> records = new HashMap<RecordKeyType, RecordValueType>();

    @Override
    public boolean hasRecord(RecordKeyType type) {
        return records.containsKey(type);
    }

    @Override
    public void setRecord(RecordKeyType key, RecordValueType value) {
        records.put(key, value);
    }

    @Override
    public boolean isTerminal() {
        return terminal;
    }


    @Override
    public Transport getTransport() {
        return transport;
    }

    @Override
    public void setTransport(Transport transport) {
        this.transport = transport;
    }

    @Override
    public void setReactor(Reactor reactor) {
        this.reactor = reactor;
    }

    // TODO: all this gets stuffed into records in the C code...
    private BaseHandler _handler = new BaseHandler();
    @Override
    public void add(Handler handler) {
        _handler.add(handler);
    }

    @Override
    public Handler getHandler() {
        return _handler;
    }
}
