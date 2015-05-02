package org.apache.qpid.proton.reactor;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import java.io.IOException;
import java.util.ArrayList;

import org.apache.qpid.proton.Proton;
import org.apache.qpid.proton.engine.BaseHandler;
import org.apache.qpid.proton.engine.Event;
import org.apache.qpid.proton.engine.Event.Type;
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
     * Tests basic operation of the Reactor.acceptor method by creating an acceptor
     * which is immediately closed by the reactor.  The expected behaviour is for:
     * <ul>
     *   <li>The reactor to end immediately (as it has no more work to process).</li>
     *   <li>The handler, associated with the acceptor, to receive init, update and
     *       final events.</li>
     *   <li>For it's lifetime, the acceptor is one of the reactor's children.</li>
     * </ul>
     * @throws IOException
     */
    @Test
    public void basicAcceptor() throws IOException {
        Reactor reactor = Proton.reactor();
        final Acceptor acceptor = reactor.acceptor("localhost", 0);
        assertNotNull(acceptor);
        assertTrue("acceptor should be one of the reactor's children", reactor.children().contains(acceptor));
        TestHandler acceptorHandler = new TestHandler();
        acceptor.add(acceptorHandler);
        reactor.getHandler().add(new BaseHandler() {
            @Override
            public void onReactorInit(Event event) {
                acceptor.close();
            }
        });
        reactor.run();
        acceptorHandler.assertEvents(Type.SELECTABLE_INIT, Type.SELECTABLE_UPDATED, Type.SELECTABLE_FINAL);
        assertFalse("acceptor should have been removed from the reactor's children", reactor.children().contains(acceptor));
    }
}
