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

import org.apache.qpid.proton.amqp.transport.DeliveryState;
import org.apache.qpid.proton.engine.Delivery;
import org.apache.qpid.proton.hawtdispatch.impl.Watch;
import org.apache.qpid.proton.hawtdispatch.impl.WatchBase;
import org.apache.qpid.proton.message.Message;
import org.apache.qpid.proton.message.ProtonJMessage;
import org.fusesource.hawtbuf.Buffer;
import org.fusesource.hawtdispatch.Task;

/**
 * @author <a href="http://hiramchirino.com">Hiram Chirino</a>
 */
public abstract class MessageDelivery extends WatchBase {

    final int initialSize;
    private Message message;
    private Buffer encoded;
    public Delivery delivery;
    private int sizeHint = 32;

    static Buffer encode(Message message, int sizeHint) {
        byte[] buffer = new byte[sizeHint];
        int size = ((ProtonJMessage)message).encode2(buffer, 0, sizeHint);
        if( size > sizeHint ) {
            buffer = new byte[size];
            size = message.encode(buffer, 0, size);
        }
        return new Buffer(buffer, 0, size);
    }

    static Message decode(Buffer buffer) {
        Message msg = Message.Factory.create();
        int offset = buffer.offset;
        int len = buffer.length;
        while( len > 0 ) {
            int decoded = msg.decode(buffer.data, offset, len);
            assert decoded > 0: "Make progress decoding the message";
            offset += decoded;
            len -= decoded;
        }
        return msg;
    }

    public MessageDelivery(Message message) {
        this(message, encode(message, 32));
    }

    public MessageDelivery(Buffer encoded) {
        this(null, encoded);
    }

    public MessageDelivery(Message message, Buffer encoded) {
        this.message = message;
        this.encoded = encoded;
        sizeHint = this.encoded.length;
        initialSize = sizeHint;
    }

    public Message getMessage() {
        if( message == null ) {
            message = decode(encoded);
        }
        return message;
    }

    public Buffer encoded() {
        if( encoded == null ) {
            encoded = encode(message, sizeHint);
            sizeHint = encoded.length;
        }
        return encoded;
    }

    public boolean isSettled() {
        return delivery!=null && delivery.isSettled();
    }

    public DeliveryState getRemoteState() {
        return delivery==null ? null : delivery.getRemoteState();
    }

    public DeliveryState getLocalState() {
        return delivery==null ? null : delivery.getLocalState();
    }

    public void onEncoded(final Callback<Void> cb) {
        addWatch(new Watch() {
            @Override
            public boolean execute() {
                if( delivery!=null ) {
                    cb.onSuccess(null);
                    return true;
                }
                return false;
            }
        });
    }

    /**
     * @return the remote delivery state when it changes.
     * @throws Exception
     */
    public DeliveryState getRemoteStateChange() throws Exception {
        AmqpEndpointBase.assertNotOnDispatchQueue();
        return getRemoteStateChangeFuture().await();
    }

    /**
     * @return the future remote delivery state when it changes.
     */
    public Future<DeliveryState> getRemoteStateChangeFuture() {
        final Promise<DeliveryState> rc = new Promise<DeliveryState>();
        link().queue().execute(new Task() {
            @Override
            public void run() {
                onRemoteStateChange(rc);
            }
        });
        return rc;
    }

    abstract AmqpLink link();

    boolean watchingRemoteStateChange;
    public void onRemoteStateChange(final Callback<DeliveryState> cb) {
        watchingRemoteStateChange = true;
        final DeliveryState original = delivery.getRemoteState();
        addWatch(new Watch() {
            @Override
            public boolean execute() {
                if (original == null) {
                    if( delivery.getRemoteState()!=null ) {
                        cb.onSuccess(delivery.getRemoteState());
                        watchingRemoteStateChange = false;
                        return true;
                    }
                } else {
                    if( !original.equals(delivery.getRemoteState()) ) {
                        cb.onSuccess(delivery.getRemoteState());
                        watchingRemoteStateChange = false;
                        return true;
                    }
                }
                return false;
            }
        });
    }

    /**
     * @return the remote delivery state once settled.
     * @throws Exception
     */
    public DeliveryState getSettle() throws Exception {
        AmqpEndpointBase.assertNotOnDispatchQueue();
        return getSettleFuture().await();
    }

    /**
     * @return the future remote delivery state once the delivery is settled.
     */
    public Future<DeliveryState> getSettleFuture() {
        final Promise<DeliveryState> rc = new Promise<DeliveryState>();
        link().queue().execute(new Task() {
            @Override
            public void run() {
                onSettle(rc);
            }
        });
        return rc;
    }

    public void onSettle(final Callback<DeliveryState> cb) {
        addWatch(new Watch() {
            @Override
            public boolean execute() {
                if( delivery!=null && delivery.isSettled() ) {
                    cb.onSuccess(delivery.getRemoteState());
                    return true;
                }
                return false;
            }
        });
    }

    @Override
    protected void fireWatches() {
        super.fireWatches();
    }

    void incrementDeliveryCount() {
        Message msg = getMessage();
        msg.setDeliveryCount(msg.getDeliveryCount()+1);
        encoded = null;
    }

    public void redeliver(boolean incrementDeliveryCounter) {
        if( incrementDeliveryCounter ) {
            incrementDeliveryCount();
        }
    }

    public void settle() {
        if( !delivery.isSettled() ) {
            delivery.settle();
        }
    }
}
