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

package org.apache.qpid.proton.hawtdispatch.api;

import org.apache.qpid.proton.amqp.transport.ErrorCondition;
import org.apache.qpid.proton.hawtdispatch.impl.AmqpListener;
import org.apache.qpid.proton.hawtdispatch.impl.AmqpTransport;
import org.apache.qpid.proton.engine.Delivery;
import org.apache.qpid.proton.engine.Endpoint;
import org.apache.qpid.proton.engine.ProtonJConnection;
import org.apache.qpid.proton.engine.ProtonJSession;
import org.apache.qpid.proton.engine.impl.ProtocolTracer;
import org.fusesource.hawtdispatch.DispatchQueue;
import org.fusesource.hawtdispatch.Task;

import java.io.IOException;
import java.util.ArrayList;
import java.util.HashSet;

/**
 * @author <a href="http://hiramchirino.com">Hiram Chirino</a>
 */
public class AmqpConnection extends AmqpEndpointBase  {

    AmqpTransport transport;
    ProtonJConnection connection;
    HashSet<AmqpSender> senders = new HashSet<AmqpSender>();
    boolean closing = false;

    public static AmqpConnection connect(AmqpConnectOptions options) {
        return new AmqpConnection(options);
    }

    private AmqpConnection(AmqpConnectOptions options) {
        transport = AmqpTransport.connect(options);
        transport.setListener(new AmqpListener() {
            @Override
            public void processDelivery(Delivery delivery) {
                Attachment attachment = (Attachment) getTransport().context(delivery.getLink()).getAttachment();
                AmqpLink link = (AmqpLink) attachment.endpoint();
                link.processDelivery(delivery);
            }

            @Override
            public void processRefill() {
                for(AmqpSender sender: new ArrayList<AmqpSender>(senders)) {
                    sender.pumpDeliveries();
                }
                pumpOut();
            }

            public void processTransportFailure(final IOException e) {
            }
        });
        connection = transport.connection();
        connection.open();
        attach();
    }

    public void waitForConnected() throws Exception {
        assertNotOnDispatchQueue();
        getConnectedFuture().await();
    }

    public Future<Void> getConnectedFuture() {
        final Promise<Void> rc = new Promise<Void>();
        queue().execute(new Task() {
            @Override
            public void run() {
                onConnected(rc);
            }
        });
        return rc;
    }

    public void onConnected(Callback<Void> cb) {
        transport.onTransportConnected(cb);
    }

    @Override
    protected Endpoint getEndpoint() {
        return connection;
    }

    @Override
    protected AmqpConnection getConnection() {
        return this;
    }

    @Override
    protected AmqpEndpointBase getParent() {
        return null;
    }

    public AmqpSession createSession() {
        assertExecuting();
        ProtonJSession session = connection.session();
        session.open();
        pumpOut();
        return new AmqpSession(this, session);
    }

    public int getMaxSessions() {
        return connection.getMaxChannels();
    }

    public void disconnect() {
        closing = true;
        transport.disconnect();
    }

    public void waitForDisconnected() throws Exception {
        assertNotOnDispatchQueue();
        getDisconnectedFuture().await();
    }

    public Future<Void> getDisconnectedFuture() {
        final Promise<Void> rc = new Promise<Void>();
        queue().execute(new Task() {
            @Override
            public void run() {
                onDisconnected(rc);
            }
        });
        return rc;
    }

    public void onDisconnected(Callback<Void> cb) {
        transport.onTransportDisconnected(cb);
    }

    public TransportState getTransportState() {
        return transport.getState();
    }

    public Throwable getTransportFailure() {
        return transport.getFailure();
    }

    public Future<Throwable> getTransportFailureFuture() {
        final Promise<Throwable> rc = new Promise<Throwable>();
        queue().execute(new Task() {
            @Override
            public void run() {
                onTransportFailure(rc);
            }
        });
        return rc;
    }

    public void onTransportFailure(Callback<Throwable> cb) {
        transport.onTransportFailure(cb);
    }

    @Override
    public DispatchQueue queue() {
        return super.queue();
    }

    public void setProtocolTracer(ProtocolTracer protocolTracer) {
        transport.setProtocolTracer(protocolTracer);
    }

    public ProtocolTracer getProtocolTracer() {
        return transport.getProtocolTracer();
    }

    /**
     * Once the remote end, closes the transport is disconnected.
     */
    @Override
    public void close() {
        super.close();
        onRemoteClose(new Callback<ErrorCondition>() {
            @Override
            public void onSuccess(ErrorCondition value) {
                disconnect();
            }

            @Override
            public void onFailure(Throwable value) {
                disconnect();
            }
        });
    }
}
