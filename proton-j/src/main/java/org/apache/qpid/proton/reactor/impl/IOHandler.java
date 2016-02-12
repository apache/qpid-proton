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
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.channels.Channel;
import java.nio.channels.SocketChannel;
import java.util.Iterator;

import org.apache.qpid.proton.Proton;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.transport.ErrorCondition;
import org.apache.qpid.proton.engine.BaseHandler;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.EndpointState;
import org.apache.qpid.proton.engine.Event;
import org.apache.qpid.proton.engine.Sasl;
import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.engine.impl.TransportImpl;
import org.apache.qpid.proton.engine.WebSocket;
import org.apache.qpid.proton.reactor.Reactor;
import org.apache.qpid.proton.reactor.Selectable;
import org.apache.qpid.proton.reactor.Selectable.Callback;
import org.apache.qpid.proton.reactor.Selector;

public class IOHandler extends BaseHandler {

    // pni_handle_quiesced from connection.c
    private void handleQuiesced(Reactor reactor, Selector selector) throws IOException {
        // check if we are still quiesced, other handlers of
        // PN_REACTOR_QUIESCED could have produced more events to process
        if (!reactor.quiesced()) return;
        selector.select(reactor.getTimeout());
        reactor.mark();
        Iterator<Selectable> selectables = selector.readable();
        while(selectables.hasNext()) {
            selectables.next().readable();
        }
        selectables = selector.writeable();
        while(selectables.hasNext()) {
            selectables.next().writeable();
        }
        selectables = selector.expired();
        while(selectables.hasNext()) {
            selectables.next().expired();
        }
        selectables = selector.error();
        while(selectables.hasNext()) {
            selectables.next().error();
        }
        reactor.yield();
    }

    // pni_handle_open(...) from connection.c
    private void handleOpen(Event event) {
        Connection connection = event.getConnection();
        if (connection.getRemoteState() != EndpointState.UNINITIALIZED) {
            return;
        }
        Transport transport = Proton.transport();
        Sasl sasl = transport.sasl();
        sasl.client();
        sasl.setMechanisms("ANONYMOUS");
        WebSocket webSocket = transport.webSocket(null, false);
        transport.bind(connection);
    }

    // pni_handle_bound(...) from connection.c
    private void handleBound(Reactor reactor, Event event) {
        Connection connection = event.getConnection();
        String hostname = connection.getHostname();
        if (hostname == null || hostname.equals("")) {
            return;
        }

        int colonIndex = hostname.indexOf(':');
        int port = 5672;
        if (colonIndex >= 0) {
            try {
                port = Integer.parseInt(hostname.substring(colonIndex+1));
            } catch(NumberFormatException nfe) {
                throw new IllegalArgumentException("Not a valid host: " + hostname, nfe);
            }
            hostname = hostname.substring(0, colonIndex);
        }

        Transport transport = event.getConnection().getTransport();
        Socket socket = null;   // In this case, 'null' is the proton-j equivalent of PN_INVALID_SOCKET
        try {
            SocketChannel socketChannel = ((ReactorImpl)reactor).getIO().socketChannel();
            socketChannel.configureBlocking(false);
            socketChannel.connect(new InetSocketAddress(hostname, port));
            socket = socketChannel.socket();
        } catch(IOException ioException) {
            ErrorCondition condition = new ErrorCondition();
            condition.setCondition(Symbol.getSymbol("proton:io"));
            condition.setDescription(ioException.getMessage());
            transport.setCondition(condition);
            transport.close_tail();
            transport.close_head();
            transport.pop(transport.pending());   // Force generation of TRANSPORT_HEAD_CLOSE (not in C code)
        }
        selectableTransport(reactor, socket, transport);
    }

    // pni_connection_capacity from connection.c
    private static int capacity(Selectable selectable) {
        Transport transport = ((SelectableImpl)selectable).getTransport();
        int capacity = transport.capacity();
        if (capacity < 0) {
            if (transport.isClosed()) {
                selectable.terminate();
            }
        }
        return capacity;
    }

    // pni_connection_pending from connection.c
    private static int pending(Selectable selectable) {
        Transport transport = ((SelectableImpl)selectable).getTransport();
        int pending = transport.pending();
        if (pending < 0) {
            if (transport.isClosed()) {
                selectable.terminate();
            }
        }
        return pending;
    }

    // pni_connection_deadline from connection.c
    private static long deadline(SelectableImpl selectable) {
        Reactor reactor = selectable.getReactor();
        Transport transport = selectable.getTransport();
        long deadline = transport.tick(reactor.now());
        return deadline;
    }

    // pni_connection_update from connection.c
    private static void update(Selectable selectable) {
        SelectableImpl selectableImpl = (SelectableImpl)selectable;
        int c = capacity(selectableImpl);
        int p = pending(selectableImpl);
        selectable.setReading(c > 0);
        selectable.setWriting(p > 0);
        selectable.setDeadline(deadline(selectableImpl));
    }

    // pni_connection_readable from connection.c
    private static Callback connectionReadable = new Callback() {
        @Override
        public void run(Selectable selectable) {
            Reactor reactor = selectable.getReactor();
            Transport transport = ((SelectableImpl)selectable).getTransport();
            int capacity = transport.capacity();
            if (capacity > 0) {
                SocketChannel socketChannel = (SocketChannel)selectable.getChannel();
                try {
                    int n = socketChannel.read(transport.tail());
                    if (n == -1) {
                        transport.close_tail();
                    } else {
                        transport.process();
                    }
                } catch (IOException e) {
                    ErrorCondition condition = new ErrorCondition();
                    condition.setCondition(Symbol.getSymbol("proton:io"));
                    condition.setDescription(e.getMessage());
                    transport.setCondition(condition);
                    transport.close_tail();
                }
            }
            // (Comment from C code:) occasionally transport events aren't
            // generated when expected, so the following hack ensures we
            // always update the selector
            update(selectable);
            reactor.update(selectable);
        }
    };

    // pni_connection_writable from connection.c
    private static Callback connectionWritable = new Callback() {
        @Override
        public void run(Selectable selectable) {
            Reactor reactor = selectable.getReactor();
            Transport transport = ((SelectableImpl)selectable).getTransport();
            int pending = transport.pending();
            if (pending > 0) {
                SocketChannel channel = (SocketChannel)selectable.getChannel();
                try {
                    int n = channel.write(transport.head());
                    if (n < 0) {
                        transport.close_head();
                    } else {
                        transport.pop(n);
                    }
                } catch(IOException ioException) {
                    ErrorCondition condition = new ErrorCondition();
                    condition.setCondition(Symbol.getSymbol("proton:io"));
                    condition.setDescription(ioException.getMessage());
                    transport.setCondition(condition);
                    transport.close_head();
                }
            }

            int newPending = transport.pending();
            if (newPending != pending) {
                update(selectable);
                reactor.update(selectable);
            }
        }
    };

    // pni_connection_error from connection.c
    private static Callback connectionError = new Callback() {
        @Override
        public void run(Selectable selectable) {
            Reactor reactor = selectable.getReactor();
            selectable.terminate();
            reactor.update(selectable);
        }
    };

    // pni_connection_expired from connection.c
    private static Callback connectionExpired = new Callback() {
        @Override
        public void run(Selectable selectable) {
            Reactor reactor = selectable.getReactor();
            Transport transport = ((SelectableImpl)selectable).getTransport();
            long deadline = transport.tick(reactor.now());
            selectable.setDeadline(deadline);
            int c = capacity(selectable);
            int p = pending(selectable);
            selectable.setReading(c > 0);
            selectable.setWriting(p > 0);
            reactor.update(selectable);
        }
    };

    private static Callback connectionFree = new Callback() {
        @Override
        public void run(Selectable selectable) {
            Channel channel = selectable.getChannel();
            if (channel != null) {
                try {
                    channel.close();
                } catch(IOException ioException) {
                    // Ignore
                }
            }
        }
    };

    // pn_reactor_selectable_transport
    // Note the socket argument can, validly be 'null' this is the equivalent of proton-c's PN_INVALID_SOCKET
    protected static Selectable selectableTransport(Reactor reactor, Socket socket, Transport transport) {
        Selectable selectable = reactor.selectable();
        selectable.setChannel(socket != null ? socket.getChannel() : null);
        selectable.onReadable(connectionReadable);
        selectable.onWritable(connectionWritable);
        selectable.onError(connectionError);
        selectable.onExpired(connectionExpired);
        selectable.onFree(connectionFree);
        ((SelectableImpl)selectable).setTransport(transport);
        ((TransportImpl)transport).setSelectable(selectable);
        ((TransportImpl)transport).setReactor(reactor);
        update(selectable);
        reactor.update(selectable);
        return selectable;
    }

    private void handleTransport(Reactor reactor, Event event) {
        TransportImpl transport = (TransportImpl)event.getTransport();
        Selectable selectable = transport.getSelectable();
        if (selectable != null && !selectable.isTerminal()) {
            update(selectable);
            reactor.update(selectable);
        }
    }

    @Override
    public void onUnhandled(Event event) {
        try {
            ReactorImpl reactor = (ReactorImpl)event.getReactor();
            Selector selector = reactor.getSelector();
            if (selector == null) {
                selector = new SelectorImpl(reactor.getIO());
                reactor.setSelector(selector);
            }

            Selectable selectable;
            switch(event.getType()) {
            case SELECTABLE_INIT:
                selectable = event.getSelectable();
                selector.add(selectable);
                break;
            case SELECTABLE_UPDATED:
                selectable = event.getSelectable();
                selector.update(selectable);
                break;
            case SELECTABLE_FINAL:
                selectable = event.getSelectable();
                selector.remove(selectable);
                selectable.release();
                break;
            case CONNECTION_LOCAL_OPEN:
                handleOpen(event);
                break;
            case CONNECTION_BOUND:
                handleBound(reactor, event);
                break;
            case TRANSPORT:
                handleTransport(reactor, event);
                break;
            case TRANSPORT_CLOSED:
                event.getTransport().unbind();
                break;
            case REACTOR_QUIESCED:
                handleQuiesced(reactor, selector);
                break;
            default:
                break;
            }
        } catch(IOException ioException) {
            // XXX: Might not be the right exception type, but at least the exception isn't being swallowed
            throw new ReactorInternalException(ioException);
        }
    }
}
