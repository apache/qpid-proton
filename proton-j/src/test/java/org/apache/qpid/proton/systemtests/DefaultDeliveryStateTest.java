/*
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
 */
package org.apache.qpid.proton.systemtests;

import static java.util.EnumSet.of;
import static org.apache.qpid.proton.engine.EndpointState.ACTIVE;
import static org.apache.qpid.proton.engine.EndpointState.CLOSED;
import static org.apache.qpid.proton.engine.EndpointState.UNINITIALIZED;
import static org.apache.qpid.proton.systemtests.TestLoggingHelper.bold;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.logging.Logger;

import org.apache.qpid.proton.Proton;
import org.apache.qpid.proton.amqp.messaging.Accepted;
import org.apache.qpid.proton.amqp.messaging.AmqpValue;
import org.apache.qpid.proton.amqp.messaging.Modified;
import org.apache.qpid.proton.amqp.messaging.Released;
import org.apache.qpid.proton.amqp.messaging.Source;
import org.apache.qpid.proton.amqp.messaging.Target;
import org.apache.qpid.proton.amqp.transport.ReceiverSettleMode;
import org.apache.qpid.proton.amqp.transport.SenderSettleMode;
import org.apache.qpid.proton.engine.Delivery;
import org.apache.qpid.proton.engine.Sender;
import org.apache.qpid.proton.message.Message;
import org.junit.Test;

public class DefaultDeliveryStateTest extends EngineTestBase
{
    private static final Logger LOGGER = Logger.getLogger(DefaultDeliveryStateTest.class.getName());

    private static final int BUFFER_SIZE = 4096;

    private final String _sourceAddress = getServer().containerId + "-link1-source";

    @Test
    public void testDefaultDeliveryState() throws Exception
    {
        LOGGER.fine(bold("======== About to create transports"));

        getClient().transport = Proton.transport();
        ProtocolTracerEnabler.setProtocolTracer(getClient().transport, TestLoggingHelper.CLIENT_PREFIX);

        getServer().transport = Proton.transport();
        ProtocolTracerEnabler.setProtocolTracer(getServer().transport, "            " + TestLoggingHelper.SERVER_PREFIX);

        doOutputInputCycle();

        getClient().connection = Proton.connection();
        getClient().transport.bind(getClient().connection);

        getServer().connection = Proton.connection();
        getServer().transport.bind(getServer().connection);

        LOGGER.fine(bold("======== About to open connections"));
        getClient().connection.open();
        getServer().connection.open();

        doOutputInputCycle();

        LOGGER.fine(bold("======== About to open sessions"));
        getClient().session = getClient().connection.session();
        getClient().session.open();

        pumpClientToServer();

        getServer().session = getServer().connection.sessionHead(of(UNINITIALIZED), of(ACTIVE));
        assertEndpointState(getServer().session, UNINITIALIZED, ACTIVE);

        getServer().session.open();
        assertEndpointState(getServer().session, ACTIVE, ACTIVE);

        pumpServerToClient();
        assertEndpointState(getClient().session, ACTIVE, ACTIVE);

        LOGGER.fine(bold("======== About to create reciever"));

        getClient().source = new Source();
        getClient().source.setAddress(_sourceAddress);

        getClient().target = new Target();
        getClient().target.setAddress(null);

        getClient().receiver = getClient().session.receiver("link1");
        getClient().receiver.setTarget(getClient().target);
        getClient().receiver.setSource(getClient().source);

        getClient().receiver.setReceiverSettleMode(ReceiverSettleMode.FIRST);
        getClient().receiver.setSenderSettleMode(SenderSettleMode.UNSETTLED);

        assertEndpointState(getClient().receiver, UNINITIALIZED, UNINITIALIZED);

        getClient().receiver.open();
        assertEndpointState(getClient().receiver, ACTIVE, UNINITIALIZED);

        pumpClientToServer();

        LOGGER.fine(bold("======== About to set up implicitly created sender"));

        getServer().sender = (Sender) getServer().connection.linkHead(of(UNINITIALIZED), of(ACTIVE));

        getServer().sender.setReceiverSettleMode(getServer().sender.getRemoteReceiverSettleMode());
        getServer().sender.setSenderSettleMode(getServer().sender.getRemoteSenderSettleMode());

        org.apache.qpid.proton.amqp.transport.Source serverRemoteSource = getServer().sender.getRemoteSource();
        getServer().sender.setSource(serverRemoteSource);

        assertEndpointState(getServer().sender, UNINITIALIZED, ACTIVE);
        getServer().sender.open();

        assertEndpointState(getServer().sender, ACTIVE, ACTIVE);

        pumpServerToClient();

        assertEndpointState(getClient().receiver, ACTIVE, ACTIVE);

        int messagCount = 3;
        getClient().receiver.flow(messagCount);

        pumpClientToServer();

        LOGGER.fine(bold("======== About to create messages and send to the client"));

        Delivery serverDelivery1 = sendMessageToClient("delivery1", "Msg1");
        Delivery serverDelivery2 = sendMessageToClient("delivery2", "Msg2");
        Delivery serverDelivery3 = sendMessageToClient("delivery3", "Msg3");

        pumpServerToClient();

        assertNull(serverDelivery1.getLocalState());
        assertNull(serverDelivery2.getLocalState());
        assertNull(serverDelivery3.getLocalState());

        assertNull(serverDelivery1.getRemoteState());
        assertNull(serverDelivery2.getRemoteState());
        assertNull(serverDelivery3.getRemoteState());

        LOGGER.fine(bold("======== About to process the messages on the client"));

        Delivery clientDelivery1 = receiveMessageFromServer("delivery1", "Msg1");
        Delivery clientDelivery2 = receiveMessageFromServer("delivery2", "Msg2");
        Delivery clientDelivery3 = receiveMessageFromServer("delivery3", "Msg3");

        // Give them some default state
        clientDelivery1.setDefaultDeliveryState(Released.getInstance());
        clientDelivery2.setDefaultDeliveryState(Released.getInstance());
        clientDelivery3.setDefaultDeliveryState(Released.getInstance());

        assertEquals(Released.getInstance(), clientDelivery1.getDefaultDeliveryState());
        assertEquals(Released.getInstance(), clientDelivery2.getDefaultDeliveryState());
        assertEquals(Released.getInstance(), clientDelivery3.getDefaultDeliveryState());

        // Check the default state doesn't influence the actual state
        assertNull(clientDelivery1.getLocalState());
        assertNull(clientDelivery2.getLocalState());
        assertNull(clientDelivery3.getLocalState());

        assertNull(clientDelivery1.getRemoteState());
        assertNull(clientDelivery2.getRemoteState());
        assertNull(clientDelivery3.getRemoteState());

        // Accept one for real, update default on another, leave last untouched
        clientDelivery1.disposition(Accepted.getInstance());
        clientDelivery2.setDefaultDeliveryState(new Modified());

        // Confirm default and actual states have or have not changed as expected
        assertEquals(Released.getInstance(), clientDelivery1.getDefaultDeliveryState());
        assertTrue(clientDelivery2.getDefaultDeliveryState() instanceof Modified);
        assertEquals(Released.getInstance(), clientDelivery3.getDefaultDeliveryState());

        assertEquals(Accepted.getInstance(), clientDelivery1.getLocalState());
        assertNull(clientDelivery2.getLocalState());
        assertNull(clientDelivery3.getLocalState());

        // Verify the server gets intended state changes
        pumpClientToServer();

        assertEquals(Accepted.getInstance(), serverDelivery1.getRemoteState());
        assertNull(serverDelivery2.getRemoteState());
        assertNull(serverDelivery3.getRemoteState());

        // Confirm server sees the default states for second and third
        // messages when they get settled during link free
        getClient().receiver.close();
        assertEndpointState(getClient().receiver, CLOSED, ACTIVE);

        pumpClientToServer();

        assertEndpointState(getServer().sender, ACTIVE, CLOSED);
        getServer().sender.close();

        assertEndpointState(getServer().sender, CLOSED, CLOSED);

        pumpServerToClient();

        getClient().receiver.free();

        assertEndpointState(getClient().receiver, CLOSED, CLOSED);

        pumpClientToServer();

        assertEquals(Accepted.getInstance(), serverDelivery1.getRemoteState());
        assertTrue(serverDelivery2.getRemoteState() instanceof Modified);
        assertEquals(Released.getInstance(), serverDelivery3.getRemoteState());
    }

    private Delivery receiveMessageFromServer(String deliveryTag, String messageContent)
    {
        Delivery delivery = getClient().connection.getWorkHead();

        assertTrue(Arrays.equals(deliveryTag.getBytes(StandardCharsets.UTF_8), delivery.getTag()));
        assertEquals("The received delivery should be on our receiver",
                            getClient().receiver, delivery.getLink());

        assertNull(delivery.getLocalState());
        assertNull(delivery.getRemoteState());

        assertFalse(delivery.isPartial());
        assertTrue(delivery.isReadable());

        byte[] received = new byte[BUFFER_SIZE];
        int len = getClient().receiver.recv(received, 0, BUFFER_SIZE);

        assertTrue("given array was too small", len < BUFFER_SIZE);

        Message m = Proton.message();
        m.decode(received, 0, len);

        Object messageBody = ((AmqpValue)m.getBody()).getValue();
        assertEquals("Unexpected message content", messageContent, messageBody);

        boolean receiverAdvanced = getClient().receiver.advance();
        assertTrue("receiver has not advanced", receiverAdvanced);

        return delivery;
    }

    private Delivery sendMessageToClient(String deliveryTag, String messageBody)
    {
        byte[] tag = deliveryTag.getBytes(StandardCharsets.UTF_8);

        Message m = Proton.message();
        m.setBody(new AmqpValue(messageBody));

        byte[] encoded = new byte[BUFFER_SIZE];
        int len = m.encode(encoded, 0, BUFFER_SIZE);

        assertTrue("given array was too small", len < BUFFER_SIZE);

        Delivery serverDelivery = getServer().sender.delivery(tag);
        int sent = getServer().sender.send(encoded, 0, len);

        assertEquals("sender unable to send all data at once as assumed for simplicity", len, sent);

        boolean senderAdvanced = getServer().sender.advance();
        assertTrue("sender has not advanced", senderAdvanced);

        return serverDelivery;
    }
}
