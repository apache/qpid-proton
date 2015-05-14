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

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import java.io.IOException;
import java.util.ArrayList;

import org.apache.qpid.proton.Proton;
import org.apache.qpid.proton.engine.BaseHandler;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Delivery;
import org.apache.qpid.proton.engine.Event;
import org.apache.qpid.proton.engine.Event.Type;
import org.apache.qpid.proton.engine.Handler;
import org.apache.qpid.proton.engine.Sender;
import org.apache.qpid.proton.engine.Session;
import org.apache.qpid.proton.reactor.impl.AcceptorImpl;
import org.junit.Test;

public class ReactorTest {

    /**
     * Tests that creating a reactor and running it:
     * <ul>
     *   <li>Doesn't throw any exceptions.</li>
     *   <li>Returns immediately from the run method (as there is no more work to do).</li>
     * </ul>
     * @throws IOException
     */
    @Test
    public void runEmpty() throws IOException {
        Reactor reactor = Proton.reactor();
        assertNotNull(reactor);
        reactor.run();
        reactor.free();
    }

    private static class TestHandler extends BaseHandler {
        private final ArrayList<Type> actual = new ArrayList<Type>();

        @Override
        public void onUnhandled(Event event) {
            actual.add(event.getType());
        }

        public void assertEvents(Type...expected) {
            assertArrayEquals(expected, actual.toArray());
        }
    }

    /**
     * Tests adding a handler to a reactor and running the reactor.  The
     * expected behaviour is for the reactor to return, and a number of reactor-
     * related events to have been delivered to the handler.
     * @throws IOException
     */
    @Test
    public void handlerRun() throws IOException {
        Reactor reactor = Proton.reactor();
        Handler handler = reactor.getHandler();
        assertNotNull(handler);
        TestHandler testHandler = new TestHandler();
        handler.add(testHandler);
        reactor.run();
        reactor.free();
        testHandler.assertEvents(Type.REACTOR_INIT, Type.SELECTABLE_INIT, Type.SELECTABLE_UPDATED, Type.SELECTABLE_FINAL, Type.REACTOR_FINAL);
    }

    /**
     * Tests basic operation of the Reactor.connection method by creating a
     * connection from a reactor, then running the reactor.  The expected behaviour
     * is for:
     * <ul>
     *   <li>The reactor to end immediately.</li>
     *   <li>The handler associated with the connection receives an init event.</li>
     *   <li>The connection is one of the reactor's children.</li>
     * </ul>
     * @throws IOException
     */
    @Test
    public void connection() throws IOException {
        Reactor reactor = Proton.reactor();
        TestHandler connectionHandler = new TestHandler();
        Connection connection = reactor.connection(connectionHandler);
        assertNotNull(connection);
        assertTrue("connection should be one of the reactor's children", reactor.children().contains(connection));
        TestHandler reactorHandler = new TestHandler();
        reactor.getHandler().add(reactorHandler);
        reactor.run();
        reactor.free();
        reactorHandler.assertEvents(Type.REACTOR_INIT, Type.SELECTABLE_INIT, Type.SELECTABLE_UPDATED, Type.SELECTABLE_FINAL, Type.REACTOR_FINAL);
        connectionHandler.assertEvents(Type.CONNECTION_INIT);
    }

    /**
     * Tests operation of the Reactor.acceptor method by creating an acceptor
     * which is immediately closed by the reactor.  The expected behaviour is for:
     * <ul>
     *   <li>The reactor to end immediately (as it has no more work to process).</li>
     *   <li>The handler, associated with the acceptor, to receive no events.</li>
     *   <li>For it's lifetime, the acceptor is one of the reactor's children.</li>
     * </ul>
     * @throws IOException
     */
    @Test
    public void acceptor() throws IOException {
        Reactor reactor = Proton.reactor();
        final Acceptor acceptor = reactor.acceptor("127.0.0.1", 0);
        assertNotNull(acceptor);
        assertTrue("acceptor should be one of the reactor's children", reactor.children().contains(acceptor));
        TestHandler acceptorHandler = new TestHandler();
        BaseHandler.setHandler(acceptor, acceptorHandler);
        reactor.getHandler().add(new BaseHandler() {
            @Override
            public void onReactorInit(Event event) {
                acceptor.close();
            }
        });
        reactor.run();
        reactor.free();
        acceptorHandler.assertEvents();
        assertFalse("acceptor should have been removed from the reactor's children", reactor.children().contains(acceptor));
    }

    private static class ServerHandler extends TestHandler {
        private Acceptor acceptor;
        public void setAcceptor(Acceptor acceptor) {
            this.acceptor = acceptor;
        }
        @Override
        public void onConnectionRemoteOpen(Event event) {
            super.onConnectionRemoteOpen(event);
            event.getConnection().open();
        }
        @Override
        public void onConnectionRemoteClose(Event event) {
            super.onConnectionRemoteClose(event);
            acceptor.close();
            event.getConnection().close();
            event.getConnection().free();
        }
    }

    /**
     * Tests end to end behaviour of the reactor by creating an acceptor and then
     * a connection (which connects to the port the acceptor is listening on).
     * As soon as the connection is established, both the acceptor and connection
     * are closed.  The events generated by the acceptor and the connection are
     * compared to a set of expected events.
     * @throws IOException
     */
    @Test
    public void connect() throws IOException {
        Reactor reactor = Proton.reactor();

        ServerHandler sh = new ServerHandler();
        Acceptor acceptor = reactor.acceptor("127.0.0.1",  0, sh);
        final int listeningPort = ((AcceptorImpl)acceptor).getPortNumber();
        sh.setAcceptor(acceptor);

        class ClientHandler extends TestHandler {
            @Override
            public void onConnectionInit(Event event) {
                super.onConnectionInit(event);
                event.getConnection().setHostname("127.0.0.1:" + listeningPort);
                event.getConnection().open();
            }
            @Override
            public void onConnectionRemoteOpen(Event event) {
                super.onConnectionRemoteOpen(event);
                event.getConnection().close();
            }
            @Override
            public void onConnectionRemoteClose(Event event) {
                super.onConnectionRemoteClose(event);
                event.getConnection().free();
            }
        }
        ClientHandler ch = new ClientHandler();
        Connection connection = reactor.connection(ch);

        assertTrue("acceptor should be one of the reactor's children", reactor.children().contains(acceptor));
        assertTrue("connection should be one of the reactor's children", reactor.children().contains(connection));

        reactor.run();
        reactor.free();

        assertFalse("acceptor should have been removed from the reactor's children", reactor.children().contains(acceptor));
        assertFalse("connection should have been removed from the reactor's children", reactor.children().contains(connection));
        sh.assertEvents(Type.CONNECTION_INIT, Type.CONNECTION_BOUND,
                // XXX: proton-c generates a PN_TRANSPORT event here
                Type.CONNECTION_REMOTE_OPEN, Type.CONNECTION_LOCAL_OPEN,
                Type.TRANSPORT, Type.CONNECTION_REMOTE_CLOSE,
                Type.TRANSPORT_TAIL_CLOSED, Type.CONNECTION_LOCAL_CLOSE,
                Type.TRANSPORT, Type.TRANSPORT_HEAD_CLOSED,
                Type.TRANSPORT_CLOSED, Type.CONNECTION_UNBOUND,
                Type.CONNECTION_FINAL);

        ch.assertEvents(Type.CONNECTION_INIT, Type.CONNECTION_LOCAL_OPEN,
                Type.CONNECTION_BOUND,
                // XXX: proton-c generates two PN_TRANSPORT events here
                Type.CONNECTION_REMOTE_OPEN, Type.CONNECTION_LOCAL_CLOSE,
                Type.TRANSPORT, Type.TRANSPORT_HEAD_CLOSED,
                Type.CONNECTION_REMOTE_CLOSE, Type.TRANSPORT_TAIL_CLOSED,
                Type.TRANSPORT_CLOSED, Type.CONNECTION_UNBOUND,
                Type.CONNECTION_FINAL);

    }

    private static class SinkHandler extends BaseHandler {
        protected int received = 0;

        @Override
        public void onDelivery(Event event) {
            Delivery dlv = event.getDelivery();
            if (!dlv.isPartial()) {
                dlv.settle();
                ++received;
            }
        }
    }

    private static class SourceHandler extends BaseHandler {
        private int remaining;
        private final int port;

        protected SourceHandler(int count, int port) {
            remaining = count;
            this.port = port;
        }

        @Override
        public void onConnectionInit(Event event) {
            Connection conn = event.getConnection();
            conn.setHostname("127.0.0.1:" + port);
            Session ssn = conn.session();
            Sender snd = ssn.sender("sender");
            conn.open();
            ssn.open();
            snd.open();
        }

        @Override
        public void onLinkFlow(Event event) {
            Sender link = (Sender)event.getLink();
            while (link.getCredit() > 0 && remaining > 0) {
                Delivery dlv = link.delivery(new byte[0]);
                assertNotNull(dlv);
                dlv.settle();
                link.advance();
                --remaining;
            }

            if (remaining == 0) {
                event.getConnection().close();
            }
        }

        @Override
        public void onConnectionRemoteClose(Event event) {
            event.getConnection().free();
        }
    }

    private void transfer(int count, int window) throws IOException {
        Reactor reactor = Proton.reactor();
        ServerHandler sh = new ServerHandler();
        Acceptor acceptor = reactor.acceptor("127.0.0.1", 0, sh);
        sh.setAcceptor(acceptor);
        sh.add(new Handshaker());
        // XXX: a window of 1 doesn't work unless the flowcontroller is
        // added after the thing that settles the delivery
        sh.add(new FlowController(window));
        SinkHandler snk = new SinkHandler();
        sh.add(snk);

        SourceHandler src = new SourceHandler(count, ((AcceptorImpl)acceptor).getPortNumber());
        reactor.connection(src);

        reactor.run();
        reactor.free();
        assertEquals("Did not receive the expected number of messages", count, snk.received);
    }

    @Test
    public void transfer_0to64_2() throws IOException {
        for (int i = 0; i < 64; ++i) {
            transfer(i, 2);
        }
    }

    @Test
    public void transfer_1024_64() throws IOException {
        transfer(1024, 64);
    }

    @Test
    public void transfer_4096_1024() throws IOException {
        transfer(4*1024, 1024);
    }

    @Test
    public void schedule() throws IOException {
        Reactor reactor = Proton.reactor();
        TestHandler reactorHandler = new TestHandler();
        reactor.getHandler().add(reactorHandler);
        TestHandler taskHandler = new TestHandler();
        reactor.schedule(0, taskHandler);
        reactor.run();
        reactor.free();
        reactorHandler.assertEvents(Type.REACTOR_INIT, Type.SELECTABLE_INIT, Type.SELECTABLE_UPDATED, Type.REACTOR_QUIESCED, Type.SELECTABLE_UPDATED,
                Type.SELECTABLE_FINAL, Type.REACTOR_FINAL);
        taskHandler.assertEvents(Type.TIMER_TASK);
    }
}
