/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package org.apache.qpid.proton.hawtdispatch.impl;

import org.apache.qpid.proton.hawtdispatch.api.AmqpConnectOptions;
import org.apache.qpid.proton.hawtdispatch.api.Callback;
import org.apache.qpid.proton.hawtdispatch.api.ChainedCallback;
import org.apache.qpid.proton.hawtdispatch.api.TransportState;
import org.apache.qpid.proton.engine.*;
import org.apache.qpid.proton.engine.impl.ByteBufferUtils;
import org.apache.qpid.proton.engine.impl.ProtocolTracer;
import org.fusesource.hawtbuf.Buffer;
import org.fusesource.hawtbuf.DataByteArrayOutputStream;
import org.fusesource.hawtbuf.UTF8Buffer;
import org.fusesource.hawtdispatch.*;
import org.fusesource.hawtdispatch.transport.DefaultTransportListener;
import org.fusesource.hawtdispatch.transport.SslTransport;
import org.fusesource.hawtdispatch.transport.TcpTransport;
import org.fusesource.hawtdispatch.transport.Transport;

import java.io.IOException;
import java.net.URI;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.EnumSet;
import java.util.HashSet;
import java.util.LinkedList;

import static org.apache.qpid.proton.hawtdispatch.api.TransportState.*;
import static org.fusesource.hawtdispatch.Dispatch.NOOP;

/**
 * @author <a href="http://hiramchirino.com">Hiram Chirino</a>
 */
public class AmqpTransport extends WatchBase {

    private TransportState state = CREATED;

    final DispatchQueue queue;
    final ProtonJConnection connection;
    Transport hawtdispatchTransport;
    ProtonJTransport protonTransport;
    Throwable failure;
    CustomDispatchSource<Defer,LinkedList<Defer>> defers;

    public static final EnumSet<EndpointState> ALL_SET = EnumSet.allOf(EndpointState.class);

    private AmqpTransport(DispatchQueue queue) {
        this.queue = queue;
        this.connection = (ProtonJConnection) Connection.Factory.create();

        defers = Dispatch.createSource(EventAggregators.<Defer>linkedList(), this.queue);
        defers.setEventHandler(new Task(){
            public void run() {
                for( Defer defer: defers.getData() ) {
                    assert defer.defered = true;
                    defer.defered = false;
                    defer.run();
                }
            }
        });
        defers.resume();
    }

    static public AmqpTransport connect(AmqpConnectOptions options) {
        AmqpConnectOptions opts = options.clone();
        if( opts.getDispatchQueue() == null ) {
            opts.setDispatchQueue(Dispatch.createQueue());
        }
        if( opts.getBlockingExecutor() == null ) {
            opts.setBlockingExecutor(AmqpConnectOptions.getBlockingThreadPool());
        }
        return new AmqpTransport(opts.getDispatchQueue()).connecting(opts);
    }

    private AmqpTransport connecting(final AmqpConnectOptions options) {
        assert state == CREATED;
        try {
            state = CONNECTING;
            if( options.getLocalContainerId()!=null ) {
                connection.setLocalContainerId(options.getLocalContainerId());
            }
            if( options.getRemoteContainerId()!=null ) {
                connection.setContainer(options.getRemoteContainerId());
            }
            connection.setHostname(options.getHost().getHost());
            Callback<Void> onConnect = new Callback<Void>() {
                @Override
                public void onSuccess(Void value) {
                    if( state == CONNECTED ) {
                        hawtdispatchTransport.setTransportListener(new AmqpTransportListener());
                        fireWatches();
                    }
                }

                @Override
                public void onFailure(Throwable value) {
                    if( state == CONNECTED || state == CONNECTING ) {
                        failure = value;
                        disconnect();
                        fireWatches();
                    }
                }
            };
            if( options.getUser()!=null ) {
                onConnect = new SaslClientHandler(options, onConnect);
            }
            createTransport(options, onConnect);
        } catch (Throwable e) {
            failure = e;
        }
        fireWatches();
        return this;
    }

    public TransportState getState() {
        return state;
    }

    /**
     * Creates and start a transport to the AMQP server.  Passes it to the onConnect
     * once the transport is connected.
     *
     * @param onConnect
     * @throws Exception
     */
    void createTransport(AmqpConnectOptions options, final Callback<Void> onConnect) throws Exception {
        final TcpTransport transport;
        if( options.getSslContext() !=null ) {
            SslTransport ssl = new SslTransport();
            ssl.setSSLContext(options.getSslContext());
            transport = ssl;
        } else {
            transport = new TcpTransport();
        }

        URI host = options.getHost();
        if( host.getPort() == -1 ) {
            if( options.getSslContext()!=null ) {
                host = new URI(host.getScheme()+"://"+host.getHost()+":5672");
            } else {
                host = new URI(host.getScheme()+"://"+host.getHost()+":5671");
            }
        }


        transport.setBlockingExecutor(options.getBlockingExecutor());
        transport.setDispatchQueue(options.getDispatchQueue());

        transport.setMaxReadRate(options.getMaxReadRate());
        transport.setMaxWriteRate(options.getMaxWriteRate());
        transport.setReceiveBufferSize(options.getReceiveBufferSize());
        transport.setSendBufferSize(options.getSendBufferSize());
        transport.setTrafficClass(options.getTrafficClass());
        transport.setUseLocalHost(options.isUseLocalHost());
        transport.connecting(host, options.getLocalAddress());

        transport.setTransportListener(new DefaultTransportListener(){
            public void onTransportConnected() {
                if(state==CONNECTING) {
                    state = CONNECTED;
                    onConnect.onSuccess(null);
                    transport.resumeRead();
                }
            }

            public void onTransportFailure(final IOException error) {
                if(state==CONNECTING) {
                    onConnect.onFailure(error);
                }
            }

        });
        transport.connecting(host, options.getLocalAddress());
        bind(transport);
        transport.start(NOOP);
    }

    class SaslClientHandler extends ChainedCallback<Void, Void> {

        private final AmqpConnectOptions options;

        public SaslClientHandler(AmqpConnectOptions options, Callback<Void> next) {
            super(next);
            this.options = options;
        }

        public void onSuccess(final Void value) {
            final Sasl s = protonTransport.sasl();
            s.client();
            pumpOut();
            hawtdispatchTransport.setTransportListener(new AmqpTransportListener() {

                Sasl sasl = s;

                @Override
                void process() {
                    if (sasl != null) {
                        sasl = processSaslEvent(sasl);
                        if (sasl == null) {
                            // once sasl handshake is done.. we need to read the protocol header again.
                            ((AmqpProtocolCodec) hawtdispatchTransport.getProtocolCodec()).readProtocolHeader();
                        }
                    }
                }

                @Override
                public void onTransportFailure(IOException error) {
                    next.onFailure(error);
                }

                @Override
                void onFailure(Throwable error) {
                    next.onFailure(error);
                }

                boolean authSent = false;

                private Sasl processSaslEvent(Sasl sasl) {
                    if (sasl.getOutcome() == Sasl.SaslOutcome.PN_SASL_OK) {
                        next.onSuccess(null);
                        return null;
                    }
                    HashSet<String> mechanisims = new HashSet<String>(Arrays.asList(sasl.getRemoteMechanisms()));
                    if (!authSent && !mechanisims.isEmpty()) {
                        if (mechanisims.contains("PLAIN")) {
                            authSent = true;
                            DataByteArrayOutputStream os = new DataByteArrayOutputStream();
                            try {
                                os.writeByte(0);
                                os.write(new UTF8Buffer(options.getUser()));
                                os.writeByte(0);
                                if (options.getPassword() != null) {
                                    os.write(new UTF8Buffer(options.getPassword()));
                                }
                            } catch (IOException e) {
                                throw new RuntimeException(e);
                            }
                            Buffer buffer = os.toBuffer();
                            sasl.setMechanisms(new String[]{"PLAIN"});
                            sasl.send(buffer.data, buffer.offset, buffer.length);
                        } else if (mechanisims.contains("ANONYMOUS")) {
                            authSent = true;
                            sasl.setMechanisms(new String[]{"ANONYMOUS"});
                            sasl.send(new byte[0], 0, 0);
                        } else {
                            next.onFailure(Support.illegalState("Remote does not support plain password authentication."));
                            return null;
                        }
                    }
                    return sasl;
                }
            });
        }
    }

    class SaslServerListener extends AmqpTransportListener {
        Sasl sasl;

        @Override
        public void onTransportCommand(Object command) {
            try {
                if (command.getClass() == AmqpHeader.class) {
                    AmqpHeader header = (AmqpHeader)command;
                    switch( header.getProtocolId() ) {
                        case 3: // Client will be using SASL for auth..
                            if( listener!=null ) {
                                sasl = listener.processSaslConnect(protonTransport);
                                break;
                            }
                        default:
                            AmqpTransportListener listener = new AmqpTransportListener();
                            hawtdispatchTransport.setTransportListener(listener);
                            listener.onTransportCommand(command);
                            return;
                    }
                    command = header.getBuffer();
                }
            } catch (Exception e) {
                onFailure(e);
            }
            super.onTransportCommand(command);
        }

        @Override
        void process() {
            if (sasl != null) {
                sasl = listener.processSaslEvent(sasl);
            }
            if (sasl == null) {
                // once sasl handshake is done.. we need to read the protocol header again.
                ((AmqpProtocolCodec) hawtdispatchTransport.getProtocolCodec()).readProtocolHeader();
                hawtdispatchTransport.setTransportListener(new AmqpTransportListener());
            }
        }
    }

    static public AmqpTransport accept(Transport transport) {
        return new AmqpTransport(transport.getDispatchQueue()).accepted(transport);
    }

    private AmqpTransport accepted(final Transport transport) {
        state = CONNECTED;
        bind(transport);
        hawtdispatchTransport.setTransportListener(new SaslServerListener());
        return this;
    }

    private void bind(final Transport transport) {
        this.hawtdispatchTransport = transport;
        this.protonTransport = (ProtonJTransport) org.apache.qpid.proton.engine.Transport.Factory.create();
        this.protonTransport.bind(connection);
        if( transport.getProtocolCodec()==null ) {
            try {
                transport.setProtocolCodec(new AmqpProtocolCodec());
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }
    }

    public void defer(Defer defer) {
        if( !defer.defered ) {
            defer.defered = true;
            defers.merge(defer);
        }
    }

    public void pumpOut() {
        assertExecuting();
        defer(deferedPumpOut);
    }

    private Defer deferedPumpOut = new Defer() {
        public void run() {
            doPumpOut();
        }
    };

    private void doPumpOut() {
        switch(state) {
            case CONNECTING:
            case CONNECTED:
                break;
            default:
                return;
        }

        int size = hawtdispatchTransport.getProtocolCodec().getWriteBufferSize();
        byte data[] = new byte[size];
        boolean done = false;
        int pumped = 0;
        while( !done && !hawtdispatchTransport.full() ) {
            int count = protonTransport.output(data, 0, size);
            if( count > 0 ) {
                pumped += count;
                boolean accepted = hawtdispatchTransport.offer(new Buffer(data, 0, count));
                assert accepted: "Should be accepted since the transport was not full";
            } else {
                done = true;
            }
        }
        if( pumped > 0 && !hawtdispatchTransport.full() ) {
            listener.processRefill();
        }
    }

    public Sasl sasl;
    public void fireListenerEvents() {
        fireWatches();

        if( sasl!=null ) {
            sasl = listener.processSaslEvent(sasl);
            if( sasl==null ) {
                // once sasl handshake is done.. we need to read the protocol header again.
                ((AmqpProtocolCodec)this.hawtdispatchTransport.getProtocolCodec()).readProtocolHeader();
            }
        }

        context(connection).fireListenerEvents(listener);

        Session session = connection.sessionHead(ALL_SET, ALL_SET);
        while(session != null)
        {
            context(session).fireListenerEvents(listener);
            session = session.next(ALL_SET, ALL_SET);
        }

        Link link = connection.linkHead(ALL_SET, ALL_SET);
        while(link != null)
        {
            context(link).fireListenerEvents(listener);
            link = link.next(ALL_SET, ALL_SET);
        }

        Delivery delivery = connection.getWorkHead();
        while(delivery != null)
        {
            listener.processDelivery(delivery);
            delivery = delivery.getWorkNext();
        }

        listener.processRefill();
    }


    public ProtonJConnection connection() {
        return connection;
    }

    AmqpListener listener = new AmqpListener();
    public AmqpListener getListener() {
        return listener;
    }

    public void setListener(AmqpListener listener) {
        this.listener = listener;
    }

    public EndpointContext context(Endpoint endpoint) {
        EndpointContext context = (EndpointContext) endpoint.getContext();
        if( context == null ) {
            context = new EndpointContext(this, endpoint);
            endpoint.setContext(context);
        }
        return context;
    }

    class AmqpTransportListener extends DefaultTransportListener {

        @Override
        public void onTransportConnected() {
            if( listener!=null ) {
                listener.processTransportConnected();
            }
        }

        @Override
        public void onRefill() {
            if( listener!=null ) {
                listener.processRefill();
            }
        }

        @Override
        public void onTransportCommand(Object command) {
            if( state != CONNECTED ) {
                return;
            }
            try {
                Buffer buffer;
                if (command.getClass() == AmqpHeader.class) {
                    buffer = ((AmqpHeader) command).getBuffer();
                } else {
                    buffer = (Buffer) command;
                }
                ByteBuffer bbuffer = buffer.toByteBuffer();
                do {
                  ByteBuffer input = protonTransport.getInputBuffer();
                  ByteBufferUtils.pour(bbuffer, input);
                  protonTransport.processInput();
                } while (bbuffer.remaining() > 0);
                process();
                pumpOut();
            } catch (Exception e) {
                onFailure(e);
            }
        }

        void process() {
            fireListenerEvents();
        }

        @Override
        public void onTransportFailure(IOException error) {
            if( state==CONNECTED ) {
                failure = error;
                if( listener!=null ) {
                    listener.processTransportFailure(error);
                    fireWatches();
                }
            }
        }

        void onFailure(Throwable error) {
            failure = error;
            if( listener!=null ) {
                listener.processFailure(error);
                fireWatches();
            }
        }
    }

    public void disconnect() {
        assertExecuting();
        if( state == CONNECTING || state==CONNECTED) {
            state = DISCONNECTING;
            if( hawtdispatchTransport!=null ) {
                hawtdispatchTransport.stop(new Task(){
                    public void run() {
                        state = DISCONNECTED;
                        hawtdispatchTransport = null;
                        protonTransport = null;
                        fireWatches();
                    }
                });
            }
        }
    }

    public DispatchQueue queue() {
        return queue;
    }

    public void assertExecuting() {
        queue().assertExecuting();
    }

    public void onTransportConnected(final Callback<Void> cb) {
        addWatch(new Watch() {
            @Override
            public boolean execute() {
                if( failure !=null ) {
                    cb.onFailure(failure);
                    return true;
                }
                if( state!=CONNECTING ) {
                    cb.onSuccess(null);
                    return true;
                }
                return false;
            }
        });
    }

    public void onTransportDisconnected(final Callback<Void> cb) {
        addWatch(new Watch() {
            @Override
            public boolean execute() {
                if( state==DISCONNECTED ) {
                    cb.onSuccess(null);
                    return true;
                }
                return false;
            }
        });
    }

    public void onTransportFailure(final Callback<Throwable> cb) {
        addWatch(new Watch() {
            @Override
            public boolean execute() {
                if( failure!=null ) {
                    cb.onSuccess(failure);
                    return true;
                }
                return false;
            }
        });
    }

    public Throwable getFailure() {
        return failure;
    }

    public void setProtocolTracer(ProtocolTracer protocolTracer) {
        protonTransport.setProtocolTracer(protocolTracer);
    }

    public ProtocolTracer getProtocolTracer() {
        return protonTransport.getProtocolTracer();
    }
}
