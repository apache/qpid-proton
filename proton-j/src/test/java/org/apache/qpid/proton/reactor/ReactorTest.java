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
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import java.io.IOException;
import java.net.ServerSocket;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;

import junit.framework.AssertionFailedError;

import org.apache.qpid.proton.Proton;
import org.apache.qpid.proton.engine.BaseHandler;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Delivery;
import org.apache.qpid.proton.engine.Event;
import org.apache.qpid.proton.engine.Event.Type;
import org.apache.qpid.proton.engine.Handler;
import org.apache.qpid.proton.engine.HandlerException;
import org.apache.qpid.proton.engine.Sender;
import org.apache.qpid.proton.engine.Session;
import org.apache.qpid.proton.reactor.impl.AcceptorImpl;
import org.apache.qpid.proton.reactor.impl.LeakTestReactor;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameters;

@RunWith(Parameterized.class)
public class ReactorTest {

    public ReactorFactory reactorFactory;
    private Reactor reactor;

    private static interface ReactorFactory {
        Reactor newReactor() throws IOException;
    }

    // Parameterize the tests, and run them once with a reactor obtained by calling
    // 'Proton.reactor()' and once with the LeakTestReactor.
    @Parameters
    public static Collection<ReactorFactory[]> data() throws IOException {
        ReactorFactory classicReactor = new ReactorFactory() {
            @Override public Reactor newReactor() throws IOException {
                return Proton.reactor();
            }
        };
        ReactorFactory newLeakDetection = new ReactorFactory() {
            @Override public Reactor newReactor() throws IOException {
                return new LeakTestReactor();
            }
        };
        return Arrays.asList(new ReactorFactory[][]{{classicReactor}, {newLeakDetection}});
    }

    public ReactorTest(ReactorFactory reactorFactory) {
        this.reactorFactory = reactorFactory;
    }

    @Before
    public void before() throws IOException {
        reactor = reactorFactory.newReactor();
    }

    private void checkForLeaks() {
        if (reactor instanceof LeakTestReactor) {
            ((LeakTestReactor)reactor).assertNoLeaks();
        }
    }

    @After
    public void after() {
        checkForLeaks();
    }

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
        assertNotNull(reactor);
        reactor.run();
        reactor.free();
    }

    private static class TestHandler extends BaseHandler {
        private final ArrayList<Type> actual = new ArrayList<Type>();

        @Override
        public void onUnhandled(Event event) {
            assertNotNull(event.getReactor());
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
        TestHandler connectionHandler = new TestHandler();
        Connection connection = reactor.connection(connectionHandler);
        assertNotNull(connection);
        assertTrue("connection should be one of the reactor's children", reactor.children().contains(connection));
        reactor.setConnectionHost(connection, "127.0.0.1", 5672);
        assertEquals("connection address configuration failed",
                     reactor.getConnectionAddress(connection), "127.0.0.1:5672");
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
        ServerHandler sh = new ServerHandler();
        Acceptor acceptor = reactor.acceptor("127.0.0.1",  0, sh);
        final int listeningPort = ((AcceptorImpl)acceptor).getPortNumber();
        sh.setAcceptor(acceptor);

        class ClientHandler extends TestHandler {
            @Override
            public void onConnectionInit(Event event) {
                super.onConnectionInit(event);
                event.getReactor().setConnectionHost(event.getConnection(),
                                                     "127.0.0.1",
                                                     listeningPort);
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

        protected SourceHandler(int count) {
            remaining = count;
        }

        @Override
        public void onConnectionInit(Event event) {
            Connection conn = event.getConnection();
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
        reactor = reactorFactory.newReactor();
        ServerHandler sh = new ServerHandler();
        Acceptor acceptor = reactor.acceptor("127.0.0.1", 0, sh);
        sh.setAcceptor(acceptor);
        sh.add(new Handshaker());
        // XXX: a window of 1 doesn't work unless the flowcontroller is
        // added after the thing that settles the delivery
        sh.add(new FlowController(window));
        SinkHandler snk = new SinkHandler();
        sh.add(snk);

        SourceHandler src = new SourceHandler(count);
        reactor.connectionToHost("127.0.0.1", ((AcceptorImpl)acceptor).getPortNumber(),
                                 src);
        reactor.run();
        reactor.free();
        assertEquals("Did not receive the expected number of messages", count, snk.received);
        checkForLeaks();
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

    private class BarfException extends RuntimeException {
        private static final long serialVersionUID = -5891140258375562884L;
    }

    private class BarfOnSomethingHandler extends BaseHandler {
        protected final BarfException exception;

        protected BarfOnSomethingHandler(BarfException exception) {
            this.exception = exception;
        }
    }

    private class BarfOnReactorInit extends BarfOnSomethingHandler {

        protected BarfOnReactorInit(BarfException exception) {
            super(exception);
        }

        @Override
        public void onReactorInit(Event e) {
            throw exception;
        }
    }

    private class BarfOnReactorFinal extends BarfOnSomethingHandler {

        protected BarfOnReactorFinal(BarfException exception) {
            super(exception);
        }

        @Override
        public void onReactorFinal(Event event) {
            throw exception;
        }
    }

    private class BarfOnConnectionInit extends BarfOnSomethingHandler {

        protected BarfOnConnectionInit(BarfException exception) {
            super(exception);
        }

        @Override
        public void onConnectionInit(Event e) {
            throw exception;
        }
    }

    private class BarfOnSessionInit extends BarfOnSomethingHandler {

        protected BarfOnSessionInit(BarfException exception) {
            super(exception);
        }

        @Override
        public void onSessionInit(Event e) {
            throw exception;
        }
    }

    private class BarfOnLinkInit extends BarfOnSomethingHandler {

        protected BarfOnLinkInit(BarfException exception) {
            super(exception);
        }

        @Override
        public void onLinkInit(Event e) {
            throw exception;
        }
    }

    private class BarfOnTask extends BarfOnSomethingHandler {

        protected BarfOnTask(BarfException exception) {
            super(exception);
        }

        @Override
        public void onTimerTask(Event e) {
            throw exception;
        }
    }

    private void assertReactorRunBarfsOnHandler(Reactor reactor, BarfException expectedException, Handler expectedHandler) {
        try {
            reactor.run();
            throw new AssertionFailedError("Reactor.run() should have thrown an exception");
        } catch(HandlerException handlerException) {
            assertSame("Linked exception does not match expected exception", expectedException, handlerException.getCause());
            assertSame("Handler in exception does not match expected handler", expectedHandler, handlerException.getHandler());
        }
    }

    @Test
    public void barfInReactorFinal() throws IOException {
        BarfException expectedBarf = new BarfException();
        Handler expectedHandler = new BarfOnReactorFinal(expectedBarf);
        reactor.getGlobalHandler().add(expectedHandler);
        assertReactorRunBarfsOnHandler(reactor, expectedBarf, expectedHandler);
        reactor.free();
    }

    @Test
    public void barfOnGlobalSet() throws IOException {
        BarfException expectedBarf = new BarfException();
        Handler expectedHandler = new BarfOnReactorInit(expectedBarf);
        reactor.setGlobalHandler(expectedHandler);
        assertReactorRunBarfsOnHandler(reactor, expectedBarf, expectedHandler);
        reactor.free();
    }

    @Test
    public void barfOnGlobalAdd() throws IOException {
        BarfException expectedBarf = new BarfException();
        Handler expectedHandler = new BarfOnReactorInit(expectedBarf);
        reactor.getGlobalHandler().add(expectedHandler);
        assertReactorRunBarfsOnHandler(reactor, expectedBarf, expectedHandler);
        reactor.free();
    }

    @Test
    public void barfOnReactorSet() throws IOException {
        BarfException expectedBarf = new BarfException();
        Handler expectedHandler = new BarfOnReactorInit(expectedBarf);
        reactor.setHandler(expectedHandler);
        assertReactorRunBarfsOnHandler(reactor, expectedBarf, expectedHandler);
        reactor.free();
    }

    @Test
    public void barfOnReactorAdd() throws IOException {
        BarfException expectedBarf = new BarfException();
        Handler expectedHandler = new BarfOnReactorInit(expectedBarf);
        reactor.getHandler().add(expectedHandler);
        assertReactorRunBarfsOnHandler(reactor, expectedBarf, expectedHandler);
        reactor.free();
    }

    @Test
    public void barfOnConnection() throws IOException {
        BarfException expectedBarf = new BarfException();
        Handler expectedHandler = new BarfOnConnectionInit(expectedBarf);
        reactor.connection(expectedHandler);
        assertReactorRunBarfsOnHandler(reactor, expectedBarf, expectedHandler);
        reactor.free();
    }

    @Test
    public void barfOnSession() throws IOException {
        BarfException expectedBarf = new BarfException();
        Handler expectedHandler = new BarfOnSessionInit(expectedBarf);
        reactor.connection(expectedHandler).session();
        assertReactorRunBarfsOnHandler(reactor, expectedBarf, expectedHandler);
        reactor.free();
    }

    @Test
    public void barfOnLink() throws IOException {
        BarfException expectedBarf = new BarfException();
        Handler expectedHandler = new BarfOnLinkInit(expectedBarf);
        reactor.connection(expectedHandler).session().sender("barf");
        assertReactorRunBarfsOnHandler(reactor, expectedBarf, expectedHandler);
        reactor.free();
    }

    @Test
    public void barfOnSchedule() throws IOException {
        BarfException expectedBarf = new BarfException();
        Handler expectedHandler = new BarfOnTask(expectedBarf);
        reactor.schedule(0, expectedHandler);
        assertReactorRunBarfsOnHandler(reactor, expectedBarf, expectedHandler);
        reactor.free();
    }

    @Test
    public void connectionRefused() throws IOException {
        final ServerSocket serverSocket = new ServerSocket(0, 0);

        class ConnectionHandler extends TestHandler {
            @Override
            public void onConnectionInit(Event event) {
                super.onConnectionInit(event);
                Connection connection = event.getConnection();
                connection.open();
                try {
                    serverSocket.close();
                } catch(IOException e) {
                    AssertionFailedError afe = new AssertionFailedError();
                    afe.initCause(e);
                    throw afe;
                }
            }
        }
        TestHandler connectionHandler = new ConnectionHandler();
        reactor.connectionToHost("127.0.0.1", serverSocket.getLocalPort(), connectionHandler);
        reactor.run();
        reactor.free();
        serverSocket.close();
        connectionHandler.assertEvents(Type.CONNECTION_INIT, Type.CONNECTION_LOCAL_OPEN, Type.CONNECTION_BOUND, Type.TRANSPORT_ERROR, Type.TRANSPORT_TAIL_CLOSED,
                Type.TRANSPORT_HEAD_CLOSED, Type.TRANSPORT_CLOSED, Type.CONNECTION_UNBOUND, Type.TRANSPORT);
    }
}
