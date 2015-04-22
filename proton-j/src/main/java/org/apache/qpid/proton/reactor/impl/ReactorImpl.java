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
import java.nio.channels.Pipe;
import java.util.HashSet;
import java.util.Set;

import org.apache.qpid.proton.Proton;
import org.apache.qpid.proton.engine.BaseHandler;
import org.apache.qpid.proton.engine.Collector;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Event;
import org.apache.qpid.proton.engine.Event.Type;
import org.apache.qpid.proton.engine.Handler;
import org.apache.qpid.proton.engine.impl.CollectorImpl;
import org.apache.qpid.proton.engine.impl.ConnectionImpl;
import org.apache.qpid.proton.engine.impl.HandlerEndpointImpl;
import org.apache.qpid.proton.reactor.Acceptor;
import org.apache.qpid.proton.reactor.Reactor;
import org.apache.qpid.proton.reactor.ReactorChild;
import org.apache.qpid.proton.reactor.Selectable;
import org.apache.qpid.proton.reactor.Selectable.Callback;
import org.apache.qpid.proton.reactor.Selectable.RecordKeyType;
import org.apache.qpid.proton.reactor.Selectable.RecordValueType;
import org.apache.qpid.proton.reactor.Selector;
import org.apache.qpid.proton.reactor.Task;

public class ReactorImpl implements Reactor {

    /*
     *   pn_record_t *attachments;
 41   pn_io_t *io;
 42   pn_collector_t *collector;
 43   pn_handler_t *global;
 44   pn_handler_t *handler;
 45   pn_list_t *children;
 46   pn_timer_t *timer;
 47   pn_socket_t wakeup[2];
 48   pn_selectable_t *selectable;
 49   pn_event_type_t previous;
 50   pn_timestamp_t now;
 51   int selectables;
 52   int timeout;
 53   bool yield;
     */

    private Object attachment;
    private CollectorImpl collector;
    private long now;
    private long timeout;
    private Handler global;
    private Handler handler;
    private Set<ReactorChild> children;
    private int selectables;
    private boolean yield;
    private Selectable selectable;
    private Type previous;
    private Timer timer;
    private final Pipe wakeup;

    @Override
    public long mark() {
        now = System.currentTimeMillis();
        return now;
    }

    @Override
    public long now() {
        return now;
    }
/*
 * tatic void pn_reactor_initialize(pn_reactor_t *reactor) {
 68   reactor->attachments = pn_record();
 69   reactor->io = pn_io();    TODO: pn_io most literally translates to SocketFactory (and possibly also ServerSocketFactory...)
 70   reactor->collector = pn_collector();
 71   reactor->global = pn_iohandler();
 72   reactor->handler = pn_handler(NULL);
 73   reactor->children = pn_list(PN_OBJECT, 0);
 74   reactor->timer = pn_timer(reactor->collector);
 75   reactor->wakeup[0] = PN_INVALID_SOCKET;
 76   reactor->wakeup[1] = PN_INVALID_SOCKET;
 77   reactor->selectable = NULL;
 78   reactor->previous = PN_EVENT_NONE;
 79   reactor->selectables = 0;
 80   reactor->timeout = 0;
 81   reactor->yield = false;
 82   pn_reactor_mark(reactor);
 83 }
 84  */
    public ReactorImpl() throws IOException {
        collector = (CollectorImpl)Proton.collector();
        global = new IOHandler();
        handler = new BaseHandler();
        children = new HashSet<ReactorChild>();
        selectables = 0;
        timer = new Timer(collector);
        wakeup = Pipe.open();
        mark();
    }
    /*
 85 static void pn_reactor_finalize(pn_reactor_t *reactor) {
 86   for (int i = 0; i < 2; i++) {
 87     if (reactor->wakeup[i] != PN_INVALID_SOCKET) {
 88       pn_close(reactor->io, reactor->wakeup[i]);
 89     }
 90   }
 91   pn_decref(reactor->attachments);
 92   pn_decref(reactor->collector);
 93   pn_decref(reactor->global);
 94   pn_decref(reactor->handler);
 95   pn_decref(reactor->children);
 96   pn_decref(reactor->timer);
 97   pn_decref(reactor->io);
 98 }
 */

    @Override
    public void attach(Object attachment) {
        this.attachment = attachment;
    }

    @Override
    public Object attachment() {
        return attachment;
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

/* TODO
 * pn_io_t *pn_reactor_io(pn_reactor_t *reactor) {
166   assert(reactor);
167   return reactor->io;
168 }
169

 */

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
        public ReleaseCallback(ReactorImpl reactor) {
            this.reactor = reactor;
        }
        @Override
        public void run(Selectable selectable) {
            if (reactor.children.remove(selectable)) {
                --reactor.selectables;
            }
        }
    }

    @Override
    public Selectable selectable() {
        Selectable result = new SelectableImpl();
        result.setCollector(collector);
        collector.put(Type.SELECTABLE_INIT, result);
        result.setReactor(this);
        children.add(result);
        result.onRelease(new ReleaseCallback(this));
        ++selectables;
        return result;
    }



    @Override
    public void update(Selectable selectable) {
        if (!selectable.hasRecord(RecordKeyType.PNI_TERMINATED)) {
            if (selectable.isTerminal()) {
                selectable.setRecord(RecordKeyType.PNI_TERMINATED, RecordValueType.PN_VOID);
                collector.put(Type.SELECTABLE_FINAL, selectable);
            } else {
                collector.put(Type.SELECTABLE_UPDATED, selectable);
            }
        }
    }

    // TODO: pn_record_get_handler
    // TODO: pn_record_set_handler
    // TODO: pn_class_reactor
    // TODO: pn_object_reactor
    // TODO: pn_event_reactor

    // pn_event_handler - TODO: this is copied from the Reactor.java code, so might need some tweaks...
    private Handler eventHandler(Event event) {
        Handler result;
        if (event.getLink() != null) {
            result = ((HandlerEndpointImpl)event.getLink()).getHandler();
            if (result != null) return result;
        }
        if (event.getSession() != null) {
            result = ((HandlerEndpointImpl)event.getSession()).getHandler();
            if (result != null) return result;
        }
        if (event.getConnection() != null) {
            result = ((HandlerEndpointImpl)event.getConnection()).getHandler();
            if (result != null) return result;
        }
//        if (event.getTransport() != null) { // TODO: do we want to allow handlers to be added to the Transport object?
//            result = ((EndpointImpl)event.getTransport()).getHandlers();
//            if (result.hasNext()) return result;
//        }

        if (event.getTask() != null) {
            result = event.getTask().getHandler();
            if (result != null) return result;
        }

        if (event.getSelectable() != null) {
            result = event.getSelectable().getHandler();
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
        return event.getType() == Type.REACTOR_QUIESCED;
    }

    @Override
    public boolean process() {
        mark();
        Type previous = null;
        while (true) {
            Event event = collector.peek();
            if (event != null) {
                if (yield) {
                    yield = false;
                    return true;
                }
                yield = false;  // TODO: is this required?
                Handler handler = eventHandler(event);
                event.dispatch(handler);
                event.dispatch(global);

                if (event.getType() == Type.CONNECTION_FINAL) { // TODO: this should be the same as the pni_reactor_dispatch_post logic...
                    children.remove(event.getConnection());
                }
                this.previous = event.getType();
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


    @Override
    public void wakeup() throws IOException {
        //selector.wakeup();
        wakeup.sink().write(ByteBuffer.allocate(1));    // TODO: c version returns a value!
    }

    @Override
    public void start() {
        collector.put(Type.REACTOR_INIT, this);
        selectable = timerSelectable();
        //selectable.setDeadline(now + timeout);      // TODO: this isn't in the C code...
    }

    @Override
    public void stop() {
        collector.put(Type.REACTOR_FINAL, this);
        // (Comment from C code) XXX: should consider removing this fron stop to avoid reentrance
        process();
        collector = null;
    }

    private boolean more() {
        return timer.tasks() > 0 || selectables > 1;
    }

    @Override
    public void run() {
        setTimeout(3141);   // TODO: eh?
        start();
        while(process()) {}
        stop();
    }

    // pn_reactor_schedule from reactor.c
    @Override
    public Task schedule(int delay, Handler handler) {
        Task task = timer.schedule(now + delay);
        task.setReactor(this);
        task.setHandler(handler);
        if (selectable != null) {
            selectable.setDeadline(timer.deadline());
            update(selectable);
        }
        return task;
    }
    // TODO: acceptor
    // TODO: connection
    // TODO: acceptorClose

    private class TimerReadable implements Callback {

        @Override
        public void run(Selectable selectable) {
            // TODO: the implication is that this will be called when the selectable is woken-up
/*
  434 static void pni_timer_readable(pn_selectable_t *sel) {
  435   char buf[64];
  436   pn_reactor_t *reactor = pni_reactor(sel);
  437   pn_socket_t fd = pn_selectable_get_fd(sel);
  438   pn_read(reactor->io, fd, buf, 64);
  439   pni_timer_expired(sel);
  440 }
 */
            // TODO: this could be more elegant...
            new TimerExpired().run(selectable);
        }

    }

    private class TimerExpired implements Callback {
        @Override
        public void run(Selectable selectable) {
            ReactorImpl reactor = (ReactorImpl) selectable.getReactor();
            reactor.timer.tick(reactor.now);
            selectable.setDeadline(reactor.timer.deadline());
            reactor.update(selectable);
        }
    }


    // pni_timer_finalize from reactor.c
    private class TimerFinalize implements Callback {
        @Override
        public void run(Selectable selectable) {
            try {
                selectable.getChannel().close();
            } catch(IOException e) {
                e.printStackTrace();
                // TODO: no idea what to do here...
            }
        }
    }

    private Selectable timerSelectable() {
        Selectable sel = selectable();
        sel.setChannel(wakeup.source());
        sel.onReadable(new TimerReadable());
        sel.onExpired(new TimerExpired());
        sel.onFinalize(new TimerFinalize());    // TODO: not sure the corresponding sel._finalize() gets called anywhere...
        sel.setReading(true);
        sel.setDeadline(timer.deadline());
        update(sel);
        return sel;
    }

    // TODO: the C code allows records to be associated with a Reactor and the Selector is get/set using that capability.
    private Selector selector;

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
        connection.add(handler);
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
}
