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
package org.apache.qpid.proton.examples;

import org.apache.qpid.proton.engine.BaseHandler;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Collector;
import org.apache.qpid.proton.engine.EndpointState;
import org.apache.qpid.proton.engine.Event;
import org.apache.qpid.proton.engine.Handler;
import org.apache.qpid.proton.engine.Sasl;
import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.engine.TransportException;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.StandardSocketOptions;
import java.nio.ByteBuffer;
import java.nio.channels.Selector;
import java.nio.channels.SelectionKey;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;


/**
 * Driver
 *
 */

public class Driver extends BaseHandler
{

    final private Collector collector;
    final private Handler[] handlers;
    final private Selector selector;

    public Driver(Collector collector, Handler ... handlers) throws IOException {
        this.collector = collector;
        this.handlers = handlers;
        this.selector = Selector.open();
    }

    public void listen(String host, int port) throws IOException {
        new Acceptor(host, port);
    }

    public void run() throws IOException {
        while (true) {
            processEvents();

            // I don't know if there is a better way to do this, but
            // the only way canceled selection keys are removed from
            // the key set is via a select operation, so we do this
            // first to figure out whether we should exit. Without
            // this we would block indefinitely when there are only
            // cancelled keys remaining.
            selector.selectNow();
            if (selector.keys().isEmpty()) {
                selector.close();
                return;
            }

            selector.selectedKeys().clear();
            selector.select();

            for (SelectionKey key : selector.selectedKeys()) {
                Selectable selectable = (Selectable) key.attachment();
                selectable.selected();
            }
        }
    }

    public void processEvents() {
        while (true) {
            Event ev = collector.peek();
            if (ev == null) break;
            ev.dispatch(this);
            for (Handler h : handlers) {
                ev.dispatch(h);
            }
            collector.pop();
        }
    }

    @Override
    public void onTransport(Event evt) {
        Transport transport = evt.getTransport();
        ChannelHandler ch = (ChannelHandler) transport.getContext();
        ch.selected();
    }

    @Override
    public void onConnectionLocalOpen(Event evt) {
        Connection conn = evt.getConnection();
        if (conn.getRemoteState() == EndpointState.UNINITIALIZED) {
            try {
                new Connector(conn);
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }
    }

    private interface Selectable {
        void selected() throws IOException;
    }

    private class Acceptor implements Selectable {

        final private ServerSocketChannel socket;
        final private SelectionKey key;

        Acceptor(String host, int port) throws IOException {
            socket = ServerSocketChannel.open();
            socket.configureBlocking(false);
            socket.bind(new InetSocketAddress(host, port));
            socket.setOption(StandardSocketOptions.SO_REUSEADDR, true);
            key = socket.register(selector, SelectionKey.OP_ACCEPT, this);
        }

        public void selected() throws IOException {
            SocketChannel sock = socket.accept();
            System.out.println("ACCEPTED: " + sock);
            Connection conn = Connection.Factory.create();
            conn.collect(collector);
            Transport transport = Transport.Factory.create();
            Sasl sasl = transport.sasl();
            sasl.setMechanisms("ANONYMOUS");
            sasl.server();
            sasl.done(Sasl.PN_SASL_OK);
            transport.bind(conn);
            new ChannelHandler(sock, SelectionKey.OP_READ, transport);
        }
    }

    private class ChannelHandler implements Selectable {

        final SocketChannel socket;
        final SelectionKey key;
        final Transport transport;

        ChannelHandler(SocketChannel socket, int ops, Transport transport) throws IOException {
            this.socket = socket;
            socket.configureBlocking(false);
            key = socket.register(selector, ops, this);
            this.transport = transport;
            transport.setContext(this);
        }

        boolean update() {
            if (socket.isConnected()) {
                int c = transport.capacity();
                int p = transport.pending();
                if (key.isValid()) {
                    key.interestOps((c != 0 ? SelectionKey.OP_READ : 0) |
                                    (p > 0 ? SelectionKey.OP_WRITE : 0));
                }
                if (c < 0 && p < 0) {
                    return true;
                } else {
                    return false;
                }
            } else {
                return false;
            }
        }

        public void selected() {
            if (!key.isValid()) { return; }

            try {
                if (key.isConnectable()) {
                    System.out.println("CONNECTED: " + socket);
                    socket.finishConnect();
                }

                if (key.isReadable()) {
                    int c = transport.capacity();
                    if (c > 0) {
                        ByteBuffer tail = transport.tail();
                        int n = socket.read(tail);
                        if (n > 0) {
                            try {
                                transport.process();
                            } catch (TransportException e) {
                                e.printStackTrace();
                            }
                        } else if (n < 0) {
                            transport.close_tail();
                        }
                    }
                }

                if (key.isWritable()) {
                    int p = transport.pending();
                    if (p > 0) {
                        ByteBuffer head = transport.head();
                        int n = socket.write(head);
                        if (n > 0) {
                            transport.pop(n);
                        } else if (n < 0) {
                            transport.close_head();
                        }
                    }
                }

                if (update()) {
                    transport.unbind();
                    System.out.println("CLOSING: " + socket);
                    socket.close();
                }
            } catch (IOException e) {
                transport.unbind();
                System.out.println(String.format("CLOSING(%s): %s", e, socket));
                try {
                    socket.close();
                } catch (IOException e2) {
                    throw new RuntimeException(e2);
                }
            }

        }

    }

    private static Transport makeTransport(Connection conn) {
        Transport transport = Transport.Factory.create();
        Sasl sasl = transport.sasl();
        sasl.setMechanisms("ANONYMOUS");
        sasl.client();
        transport.bind(conn);
        return transport;
    }

    private class Connector extends ChannelHandler {

        Connector(Connection conn) throws IOException {
            super(SocketChannel.open(), SelectionKey.OP_CONNECT, makeTransport(conn));
            System.out.println("CONNECTING: " + conn.getHostname());
            socket.connect(new InetSocketAddress(conn.getHostname(), 5672));
        }
    }

}
