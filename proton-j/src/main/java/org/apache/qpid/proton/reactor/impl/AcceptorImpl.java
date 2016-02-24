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
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;

import org.apache.qpid.proton.Proton;
import org.apache.qpid.proton.engine.BaseHandler;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Handler;
import org.apache.qpid.proton.engine.Record;
import org.apache.qpid.proton.engine.Sasl;
import org.apache.qpid.proton.engine.Sasl.SaslOutcome;
import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.engine.impl.RecordImpl;
import org.apache.qpid.proton.reactor.Acceptor;
import org.apache.qpid.proton.reactor.Reactor;
import org.apache.qpid.proton.reactor.Selectable;
import org.apache.qpid.proton.reactor.Selectable.Callback;

public class AcceptorImpl implements Acceptor {

    private Record attachments = new RecordImpl();
    private final SelectableImpl sel;

    private class AcceptorReadable implements Callback {
        @Override
        public void run(Selectable selectable) {
            Reactor reactor = selectable.getReactor();
            try {
                SocketChannel socketChannel = ((ServerSocketChannel)selectable.getChannel()).accept();
                if (socketChannel == null) {
                    throw new ReactorInternalException("Selectable readable, but no socket to accept");
                }
                Handler handler = BaseHandler.getHandler(AcceptorImpl.this);
                if (handler == null) {
                    handler = reactor.getHandler();
                }
                Connection conn = reactor.connection(handler);
                Transport trans = Proton.transport();
                Sasl sasl = trans.sasl();
                sasl.server();
                sasl.setMechanisms("ANONYMOUS");
                sasl.done(SaslOutcome.PN_SASL_OK);
                trans.bind(conn);
                IOHandler.selectableTransport(reactor, socketChannel.socket(), trans);
            } catch(IOException ioException) {
                sel.error();
            }
        }
    }

    private static class AcceptorFree implements Callback {
        @Override
        public void run(Selectable selectable) {
            try {
                if (selectable.getChannel() != null) {
                    selectable.getChannel().close();
                }
            } catch(IOException ioException) {
                // Ignore - as we can't make the channel any more closed...
            }
        }
    }

    protected AcceptorImpl(Reactor reactor, String host, int port, Handler handler) throws IOException {
        ServerSocketChannel ssc = ((ReactorImpl)reactor).getIO().serverSocketChannel();
        ssc.bind(new InetSocketAddress(host, port));
        sel = ((ReactorImpl)reactor).selectable(this);
        sel.setChannel(ssc);
        sel.onReadable(new AcceptorReadable());
        sel.onFree(new AcceptorFree());
        sel.setReactor(reactor);
        BaseHandler.setHandler(this, handler);
        sel.setReading(true);
        reactor.update(sel);
    }

    @Override
    public void close() {
        if (!sel.isTerminal()) {
            Reactor reactor = sel.getReactor();
            try {
                sel.getChannel().close();
            } catch(IOException ioException) {
                // Ignore.
            }
            sel.setChannel(null);
            sel.terminate();
            reactor.update(sel);
        }
    }

    // Used for unit tests, where acceptor is bound to an ephemeral port
    public int getPortNumber() throws IOException {
        ServerSocketChannel ssc = (ServerSocketChannel)sel.getChannel();
        return ((InetSocketAddress)ssc.getLocalAddress()).getPort();
    }

    @Override
    public void free() {
        sel.free();
    }

    @Override
    public Record attachments() {
        return attachments;
    }

}
