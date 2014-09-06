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
import org.apache.qpid.proton.engine.Delivery;
import org.apache.qpid.proton.engine.Receiver;
import org.apache.qpid.proton.amqp.messaging.Accepted;
import org.fusesource.hawtbuf.Buffer;
import org.fusesource.hawtbuf.ByteArrayOutputStream;

import java.util.LinkedList;

/**
 * @author <a href="http://hiramchirino.com">Hiram Chirino</a>
 */
public class AmqpReceiver extends AmqpLink {

    final AmqpSession parent;
    final Receiver receiver;

    public AmqpReceiver(AmqpSession parent, Receiver receiver2, QoS qos) {
        this.parent = parent;
        this.receiver = receiver2;
        attach();
    }

    @Override
    protected Receiver getEndpoint() {
        return receiver;
    }
    @Override
    protected AmqpSession getParent() {
        return parent;
    }

    ByteArrayOutputStream current = new ByteArrayOutputStream();

    @Override
    protected void processDelivery(Delivery delivery) {
        if( !delivery.isReadable() ) {
            System.out.println("it was not readable!");
            return;
        }

        if( current==null ) {
            current = new ByteArrayOutputStream();
        }

        int count;
        byte data[] = new byte[1024*4];
        while( (count = receiver.recv(data, 0, data.length)) > 0 ) {
            current.write(data, 0, count);
        }

        // Expecting more deliveries..
        if( count == 0 ) {
            return;
        }

        receiver.advance();
        Buffer buffer = current.toBuffer();
        current = null;
        onMessage(delivery, buffer);

    }

    LinkedList<MessageDelivery> inbound = new LinkedList<MessageDelivery>();

    protected void onMessage(Delivery delivery, Buffer buffer) {
        MessageDelivery md = new MessageDelivery(buffer) {
            @Override
            AmqpLink link() {
                return AmqpReceiver.this;
            }

            @Override
            public void settle() {
                if( !delivery.isSettled() ) {
                    delivery.disposition(new Accepted());
                    delivery.settle();
                }
                drain();
            }
        };
        md.delivery = delivery;
        delivery.setContext(md);
        inbound.add(md);
        drainInbound();
    }

    public void drain() {
        defer(deferedDrain);
    }

    Defer deferedDrain = new Defer(){
        public void run() {
            drainInbound();
        }
    };
    int resumed = 0;

    public void resume() {
        resumed++;
    }
    public void suspend() {
        resumed--;
    }

    AmqpDeliveryListener deliveryListener;
    private void drainInbound() {
        while( deliveryListener!=null && !inbound.isEmpty() && resumed>0) {
            deliveryListener.onMessageDelivery(inbound.removeFirst());
            receiver.flow(1);
        }
    }

    public AmqpDeliveryListener getDeliveryListener() {
        return deliveryListener;
    }

    public void setDeliveryListener(AmqpDeliveryListener deliveryListener) {
        this.deliveryListener = deliveryListener;
        drainInbound();
    }
}
