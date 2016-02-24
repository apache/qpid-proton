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
import java.nio.ByteBuffer;
import java.nio.channels.ClosedChannelException;
import java.nio.channels.Pipe;
import java.util.HashSet;
import java.util.Set;

import org.apache.qpid.proton.Proton;
import org.apache.qpid.proton.engine.BaseHandler;
import org.apache.qpid.proton.engine.Collector;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Event;
import org.apache.qpid.proton.engine.Event.Type;
import org.apache.qpid.proton.engine.EventType;
import org.apache.qpid.proton.engine.Extendable;
import org.apache.qpid.proton.engine.ExtendableAccessor;
import org.apache.qpid.proton.engine.Handler;
import org.apache.qpid.proton.engine.HandlerException;
import org.apache.qpid.proton.engine.Record;
import org.apache.qpid.proton.engine.impl.CollectorImpl;
import org.apache.qpid.proton.engine.impl.ConnectionImpl;
import org.apache.qpid.proton.engine.impl.RecordImpl;
import org.apache.qpid.proton.reactor.Acceptor;
import org.apache.qpid.proton.reactor.Reactor;
import org.apache.qpid.proton.reactor.ReactorChild;
import org.apache.qpid.proton.reactor.Selectable;
import org.apache.qpid.proton.reactor.Selectable.Callback;
import org.apache.qpid.proton.reactor.Selector;
import org.apache.qpid.proton.reactor.Task;

public class ReactorImpl implements Reactor, Extendable {
    public static final ExtendableAccessor<Event, Handler> ROOT = new ExtendableAccessor<>(Handler.class);

    private CollectorImpl collector;
    private long now;
    private long timeout;
    private Handler global;
    private Handler handler;
    private Set<ReactorChild> children;
    private int selectables;
    private boolean yield;
    private Selectable selectable;
    private EventType previous;
    private Timer timer;
    private final Pipe wakeup;
    private Selector selector;
    private Record attachments;
    private final IO io;

    @Override
    public long mark() {
        now = System.currentTimeMillis();
        return now;
    }

    @Override
    public long now() {
        return now;
    }

    protected ReactorImpl(IO io) throws IOException {
        collector = (CollectorImpl)Proton.collector();
        global = new IOHandler();
        handler = new BaseHandler();
        children = new HashSet<ReactorChild>();
        selectables = 0;
        timer = new Timer(collector);
        this.io = io;
        wakeup = this.io.pipe();
        mark();
        attachments = new RecordImpl();
    }

    public ReactorImpl() throws IOException {
        this(new IOImpl());
    }

    @Override
    public void free() {
        if (wakeup.source().isOpen()) {
            try {
                wakeup.source().close();
            } catch(IOException e) {
                // Ignore.
            }
        }
        if (wakeup.sink().isOpen()) {
            try {
                wakeup.sink().close();
            } catch(IOException e) {
                // Ignore
            }
        }

        if (selector != null) {
            selector.free();
        }

        for (ReactorChild child : children) {
            child.free();
        }
    }

    @Override
    public Record attachments() {
        return attachments;
    }

    @Override
    public long getTimeout() {
        return timeout;
    }

    @Override
    public void setTimeout(long timeout) {
        this.timeout = timeout;
    }

    @Override
    public Handler getGlobalHandler() {
        return global;
    }

    @Override
    public void setGlobalHandler(Handler handler) {
        global = handler;
    }

    @Override
    public Handler getHandler() {
        return handler;
    }

    @Override
    public void setHandler(Handler handler) {
        this.handler = handler;
    }

    @Override
    public Set<ReactorChild> children() {
        return children;
    }

    @Override
    public Collector collector() {
        return collector;
    }

    private class ReleaseCallback implements Callback {
        private final ReactorImpl reactor;
        private final ReactorChild child;
        public ReleaseCallback(ReactorImpl reactor, ReactorChild child) {
            this.reactor = reactor;
            this.child = child;
        }
        @Override
        public void run(Selectable selectable) {
            if (reactor.children.remove(child)) {
                --reactor.selectables;
                child.free();
            }
        }
    }

    @Override
    public Selectable selectable() {
        return selectable(null);
    }

    public SelectableImpl selectable(ReactorChild child) {
        SelectableImpl result = new SelectableImpl();
        result.setCollector(collector);
        collector.put(Type.SELECTABLE_INIT, result);
        result.setReactor(this);
        children.add(child == null ? result : child);
        result.onRelease(new ReleaseCallback(this, child == null ? result : child));
        ++selectables;
        return result;
    }

    @Override
    public void update(Selectable selectable) {
        SelectableImpl selectableImpl = (SelectableImpl)selectable;
        if (!selectableImpl.isTerminated()) {
            if (selectableImpl.isTerminal()) {
                selectableImpl.terminated();
                collector.put(Type.SELECTABLE_FINAL, selectable);
            } else {
                collector.put(Type.SELECTABLE_UPDATED, selectable);
            }
        }
    }

    // pn_event_handler
    private Handler eventHandler(Event event) {
        Handler result;
        if (event.getLink() != null) {
            result = BaseHandler.getHandler(event.getLink());
            if (result != null) return result;
        }
        if (event.getSession() != null) {
            result = BaseHandler.getHandler(event.getSession());
            if (result != null) return result;
        }
        if (event.getConnection() != null) {
            result = BaseHandler.getHandler(event.getConnection());
            if (result != null) return result;
        }

        if (event.getTask() != null) {
            result = BaseHandler.getHandler(event.getTask());
            if (result != null) return result;
        }

        if (event.getSelectable() != null) {
            result = BaseHandler.getHandler(event.getSelectable());
            if (result != null) return result;
        }

        return handler;
    }


    @Override
    public void yield() {
        yield = true;
    }

    @Override
    public boolean quiesced() {
        Event event = collector.peek();
        if (event == null) return true;
        if (collector.more()) return false;
        return event.getEventType() == Type.REACTOR_QUIESCED;
    }

    @Override
    public boolean process() throws HandlerException {
        mark();
        EventType previous = null;
        while (true) {
            Event event = collector.peek();
            if (event != null) {
                if (yield) {
                    yield = false;
                    return true;
                }
                Handler handler = eventHandler(event);
                dispatch(event, handler);
                dispatch(event, global);

                if (event.getEventType() == Type.CONNECTION_FINAL) {
                    children.remove(event.getConnection());
                }
                this.previous = event.getEventType();
                previous = this.previous;
                collector.pop();

            } else {
                if (more()) {
                    if (previous != Type.REACTOR_QUIESCED && this.previous != Type.REACTOR_FINAL) {
                        collector.put(Type.REACTOR_QUIESCED, this);
                    } else {
                        return true;
                    }
                } else {
                    if (selectable != null) {
                        selectable.terminate();
                        update(selectable);
                        selectable = null;
                    } else {
                        return false;
                    }
                }
            }
        }
    }

    private void dispatch(Event event, Handler handler) {
        ROOT.set(event, handler);
        event.dispatch(handler);
    }

    @Override
    public void wakeup() {
        try {
            wakeup.sink().write(ByteBuffer.allocate(1));
        } catch(ClosedChannelException channelClosedException) {
            // Ignore - pipe already closed by reactor being shutdown.
        } catch(IOException ioException) {
            throw new ReactorInternalException(ioException);
        }
    }

    @Override
    public void start() {
        collector.put(Type.REACTOR_INIT, this);
        selectable = timerSelectable();
    }

    @Override
    public void stop() throws HandlerException {
        collector.put(Type.REACTOR_FINAL, this);
        // (Comment from C code) XXX: should consider removing this from stop to avoid reentrance
        process();
        collector = null;
    }

    private boolean more() {
        return timer.tasks() > 0 || selectables > 1;
    }

    @Override
    public void run() throws HandlerException {
        setTimeout(3141);
        start();
        while(process()) {}
        stop();
    }

    // pn_reactor_schedule from reactor.c
    @Override
    public Task schedule(int delay, Handler handler) {
        Task task = timer.schedule(now + delay);
        ((TaskImpl)task).setReactor(this);
        BaseHandler.setHandler(task, handler);
        if (selectable != null) {
            selectable.setDeadline(timer.deadline());
            update(selectable);
        }
        return task;
    }

    private void expireSelectable(Selectable selectable) {
        ReactorImpl reactor = (ReactorImpl) selectable.getReactor();
        reactor.timer.tick(reactor.now);
        selectable.setDeadline(reactor.timer.deadline());
        reactor.update(selectable);
    }

    private class TimerReadable implements Callback {

        @Override
        public void run(Selectable selectable) {
            try {
                wakeup.source().read(ByteBuffer.allocate(64));
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
            expireSelectable(selectable);
        }

    }

    private class TimerExpired implements Callback {
        @Override
        public void run(Selectable selectable) {
            expireSelectable(selectable);
        }
    }


    // pni_timer_finalize from reactor.c
    private static class TimerFree implements Callback {
        @Override
        public void run(Selectable selectable) {
            try {
                selectable.getChannel().close();
            } catch(IOException ioException) {
                // Ignore
            }
        }
    }

    private Selectable timerSelectable() {
        Selectable sel = selectable();
        sel.setChannel(wakeup.source());
        sel.onReadable(new TimerReadable());
        sel.onExpired(new TimerExpired());
        sel.onFree(new TimerFree());
        sel.setReading(true);
        sel.setDeadline(timer.deadline());
        update(sel);
        return sel;
    }

    protected Selector getSelector() {
        return selector;
    }

    protected void setSelector(Selector selector) {
        this.selector = selector;
    }

    // pn_reactor_connection from connection.c
    @Override
    public Connection connection(Handler handler) {
        Connection connection = Proton.connection();
        BaseHandler.setHandler(connection, handler);
        connection.collect(collector);
        children.add(connection);
        ((ConnectionImpl)connection).setReactor(this);
        return connection;
    }

    @Override
    public Acceptor acceptor(String host, int port) throws IOException {
        return this.acceptor(host, port, null);
    }

    @Override
    public Acceptor acceptor(String host, int port, Handler handler) throws IOException {
        return new AcceptorImpl(this, host, port, handler);
    }

    public IO getIO() {
        return io;
    }
}
