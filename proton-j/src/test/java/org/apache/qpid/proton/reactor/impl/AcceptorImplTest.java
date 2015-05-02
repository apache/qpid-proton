package org.apache.qpid.proton.reactor.impl;

import java.io.IOException;
import java.nio.channels.ServerSocketChannel;

import org.apache.qpid.proton.engine.Handler;
import org.apache.qpid.proton.reactor.Reactor;
import org.apache.qpid.proton.reactor.ReactorChild;
import org.apache.qpid.proton.reactor.Selectable.Callback;
import org.junit.Test;
import org.mockito.Mockito;

public class AcceptorImplTest {

    /**
     * Tests that if ServerSocketChannel.accept() throws an IOException the Acceptor will
     * call Selectable.error() on it's underlying selector.
     * @throws IOException
     */
    @Test
    public void acceptThrowsException() throws IOException {
        final Callback mockCallback = Mockito.mock(Callback.class);
        final SelectableImpl selectable = new SelectableImpl();
        selectable.onError(mockCallback);
        ReactorImpl mockReactor = Mockito.mock(ReactorImpl.class);
        Mockito.when(mockReactor.selectable(Mockito.any(ReactorChild.class))).thenReturn(selectable);
        class MockAcceptorImpl extends AcceptorImpl {

            protected MockAcceptorImpl(Reactor reactor, String host, int port, Handler handler) throws IOException {
                super(reactor, host, port, handler);
            }

            @Override
            protected ServerSocketChannel openServerSocket() throws IOException {
                ServerSocketChannel result = Mockito.mock(ServerSocketChannel.class);
                Mockito.when(result.accept()).thenThrow(new IOException());
                return result;
            }
        }
        new MockAcceptorImpl(mockReactor, "host", 1234, null);
        selectable.readable();
        Mockito.verify(mockCallback).run(selectable);
    }

    /**
     * Tests that if ServerSocketChannel.accept() returns <code>null</code> the Acceptor will
     * throw a ReactorInternalException (because the acceptor's underlying selectable should
     * not have been marked as readable, if there is no connection to accept).
     * @throws IOException
     */
    @Test(expected=ReactorInternalException.class)
    public void acceptReturnsNull() throws IOException {
        final Callback mockCallback = Mockito.mock(Callback.class);
        final SelectableImpl selectable = new SelectableImpl();
        selectable.onError(mockCallback);
        ReactorImpl mockReactor = Mockito.mock(ReactorImpl.class);
        Mockito.when(mockReactor.selectable(Mockito.any(ReactorChild.class))).thenReturn(selectable);
        class MockAcceptorImpl extends AcceptorImpl {

            protected MockAcceptorImpl(Reactor reactor, String host, int port, Handler handler) throws IOException {
                super(reactor, host, port, handler);
            }

            @Override
            protected ServerSocketChannel openServerSocket() throws IOException {
                ServerSocketChannel result = Mockito.mock(ServerSocketChannel.class);
                Mockito.when(result.accept()).thenReturn(null);
                return result;
            }
        }
        new MockAcceptorImpl(mockReactor, "host", 1234, null);
        selectable.readable();
    }
}
