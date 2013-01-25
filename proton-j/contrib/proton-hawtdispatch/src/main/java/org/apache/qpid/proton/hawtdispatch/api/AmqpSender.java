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

import org.apache.qpid.proton.hawtdispatch.impl.Defer;
import org.apache.qpid.proton.hawtdispatch.impl.Watch;
import org.apache.qpid.proton.engine.Delivery;
import org.apache.qpid.proton.engine.Sender;
import org.apache.qpid.proton.message.Message;
import org.apache.qpid.proton.amqp.messaging.Accepted;
import org.apache.qpid.proton.amqp.messaging.Modified;
import org.apache.qpid.proton.amqp.messaging.Rejected;
import org.apache.qpid.proton.amqp.messaging.Released;
import org.apache.qpid.proton.amqp.transport.DeliveryState;
import org.fusesource.hawtbuf.Buffer;

import java.io.UnsupportedEncodingException;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedList;

/**
 * @author <a href="http://hiramchirino.com">Hiram Chirino</a>
 */
public class AmqpSender extends AmqpLink {

    private  byte[] EMPTY_BYTE_ARRAY = new byte[]{};
    long nextTagId = 0;
    HashSet<byte[]> tagCache = new HashSet<byte[]>();

    final AmqpSession parent;
    private final QoS qos;
    final Sender sender;

    public AmqpSender(AmqpSession parent, Sender sender2, QoS qos) {
        this.parent = parent;
        this.sender = sender2;
        this.qos = qos;
        attach();
        getConnection().senders.add(this);
    }

    @Override
    public void close() {
        super.close();
        getConnection().senders.remove(this);
    }

    @Override
    protected Sender getEndpoint() {
        return sender;
    }

    @Override
    protected AmqpSession getParent() {
        return parent;
    }

    final LinkedList<MessageDelivery> outbound = new LinkedList<MessageDelivery>();
    long outboundBufferSize;

    public MessageDelivery send(Message message) {
        assertExecuting();
        MessageDelivery rc = new MessageDelivery(message) {
            @Override
            AmqpLink link() {
                return AmqpSender.this;
            }

            @Override
            public void redeliver(boolean incrementDeliveryCounter) {
                super.redeliver(incrementDeliveryCounter);
                outbound.add(this);
                outboundBufferSize += initialSize;
                defer(deferedPumpDeliveries);
            }
        };
        outbound.add(rc);
        outboundBufferSize += rc.initialSize;
        pumpDeliveries();
        pumpOut();
        return rc;
    }

    Buffer currentBuffer;
    Delivery currentDelivery;

    Defer deferedPumpDeliveries = new Defer() {
        public void run() {
            pumpDeliveries();
        }
    };

    public long getOverflowBufferSize() {
        return outboundBufferSize;
    }

    protected void pumpDeliveries() {
        assertExecuting();
        try {
            while(true) {
                while( currentBuffer !=null ) {
                    if( sender.getCredit() > 0 ) {
                        int sent = sender.send(currentBuffer.data, currentBuffer.offset, currentBuffer.length);
                        currentBuffer.moveHead(sent);
                        if( currentBuffer.length == 0 ) {
                            Delivery current = currentDelivery;
                            MessageDelivery md = (MessageDelivery) current.getContext();
                            currentBuffer = null;
                            currentDelivery = null;
                            if( qos == QoS.AT_MOST_ONCE ) {
                                current.settle();
                            } else {
                                sender.advance();
                            }
                            md.fireWatches();
                        }
                    } else {
                        return;
                    }
                }

                if( outbound.isEmpty() ) {
                    return;
                }

                final MessageDelivery md = outbound.removeFirst();
                outboundBufferSize -= md.initialSize;
                currentBuffer = md.encoded();
                if( qos == QoS.AT_MOST_ONCE ) {
                    currentDelivery = sender.delivery(EMPTY_BYTE_ARRAY, 0, 0);
                } else {
                    final byte[] tag = nextTag();
                    currentDelivery = sender.delivery(tag, 0, tag.length);
                }
                md.delivery = currentDelivery;
                currentDelivery.setContext(md);
            }
        } finally {
            fireWatches();
        }
    }

    @Override
    protected void processDelivery(Delivery delivery) {
        final MessageDelivery md  = (MessageDelivery) delivery.getContext();
        if( delivery.remotelySettled() ) {
            if( delivery.getTag().length > 0 ) {
                checkinTag(delivery.getTag());
            }

            final DeliveryState state = delivery.getRemoteState();
            if( state==null || state instanceof Accepted) {
                if( !delivery.remotelySettled() ) {
                    delivery.disposition(new Accepted());
                }
            } else if( state instanceof Rejected) {
                // re-deliver /w incremented delivery counter.
                md.delivery = null;
                md.incrementDeliveryCount();
                outbound.addLast(md);
            } else if( state instanceof Released) {
                // re-deliver && don't increment the counter.
                md.delivery = null;
                outbound.addLast(md);
            } else if( state instanceof Modified) {
                Modified modified = (Modified) state;
                if ( modified.getDeliveryFailed() ) {
                  // increment delivery counter..
                  md.incrementDeliveryCount();
                }
            }
            delivery.settle();
        }
        md.fireWatches();
    }

    byte[] nextTag() {
        byte[] rc;
        if (tagCache != null && !tagCache.isEmpty()) {
            final Iterator<byte[]> iterator = tagCache.iterator();
            rc = iterator.next();
            iterator.remove();
        } else {
            try {
                rc = Long.toHexString(nextTagId++).getBytes("UTF-8");
            } catch (UnsupportedEncodingException e) {
                throw new RuntimeException(e);
            }
        }
        return rc;
    }

    void checkinTag(byte[] data) {
        if( tagCache.size() < 1024 ) {
            tagCache.add(data);
        }
    }

    public void onOverflowBufferDrained(final Callback<Void> cb) {
        addWatch(new Watch() {
            @Override
            public boolean execute() {
                if (outboundBufferSize==0) {
                    cb.onSuccess(null);
                    return true;
                }
                return false;
            }
        });
    }
}
