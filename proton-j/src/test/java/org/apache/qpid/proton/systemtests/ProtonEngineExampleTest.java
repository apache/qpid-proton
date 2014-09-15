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
import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertTrue;
import static org.apache.qpid.proton.engine.EndpointState.ACTIVE;
import static org.apache.qpid.proton.engine.EndpointState.CLOSED;
import static org.apache.qpid.proton.engine.EndpointState.UNINITIALIZED;
import static org.apache.qpid.proton.systemtests.TestLoggingHelper.bold;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;

import java.util.Arrays;
import java.util.logging.Logger;

import org.apache.qpid.proton.Proton;
import org.apache.qpid.proton.amqp.messaging.Accepted;
import org.apache.qpid.proton.amqp.messaging.AmqpValue;
import org.apache.qpid.proton.amqp.messaging.Section;
import org.apache.qpid.proton.amqp.messaging.Source;
import org.apache.qpid.proton.amqp.messaging.Target;
import org.apache.qpid.proton.amqp.transport.ReceiverSettleMode;
import org.apache.qpid.proton.amqp.transport.SenderSettleMode;
import org.apache.qpid.proton.engine.Delivery;
import org.apache.qpid.proton.engine.Receiver;
import org.apache.qpid.proton.message.Message;
import org.junit.Test;

/**
 * Simple example to illustrate the use of the Engine and Message APIs.
 *
 * Implemented as a JUnit test for convenience, although the main purpose is to educate the reader
 * rather than test the code.
 *
 * To see the protocol trace, add the following line to test/resources/logging.properties:
 *
 * org.apache.qpid.proton.logging.LoggingProtocolTracer.sent.level = ALL
 *
 * and to see the byte level trace, add the following:
 *
 * org.apache.qpid.proton.systemtests.ProtonEngineExampleTest.level = ALL
 *
 * Does not illustrate use of the Messenger API.
 */
public class ProtonEngineExampleTest extends EngineTestBase
{
    private static final Logger LOGGER = Logger.getLogger(ProtonEngineExampleTest.class.getName());

    private static final int BUFFER_SIZE = 4096;

    private final String _targetAddress = getServer().containerId + "-link1-target";

    @Test
    public void test() throws Exception
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



        LOGGER.fine(bold("======== About to create sender"));

        getClient().source = new Source();
        getClient().source.setAddress(null);

        getClient().target = new Target();
        getClient().target.setAddress(_targetAddress);

        getClient().sender = getClient().session.sender("link1");
        getClient().sender.setTarget(getClient().target);
        getClient().sender.setSource(getClient().source);
        // Exactly once delivery semantics
        getClient().sender.setSenderSettleMode(SenderSettleMode.UNSETTLED);
        getClient().sender.setReceiverSettleMode(ReceiverSettleMode.SECOND);

        assertEndpointState(getClient().sender, UNINITIALIZED, UNINITIALIZED);

        getClient().sender.open();
        assertEndpointState(getClient().sender, ACTIVE, UNINITIALIZED);

        pumpClientToServer();



        LOGGER.fine(bold("======== About to set up implicitly created receiver"));

        // A real application would be interested in more states than simply ACTIVE, as there
        // exists the possibility that the link could have moved to another state already e.g. CLOSED.
        // (See pipelining).
        getServer().receiver = (Receiver) getServer().connection.linkHead(of(UNINITIALIZED), of(ACTIVE));
        // Accept the settlement modes suggested by the client
        getServer().receiver.setSenderSettleMode(getServer().receiver.getRemoteSenderSettleMode());
        getServer().receiver.setReceiverSettleMode(getServer().receiver.getRemoteReceiverSettleMode());

        org.apache.qpid.proton.amqp.transport.Target serverRemoteTarget = getServer().receiver.getRemoteTarget();
        assertTerminusEquals(getClient().target, serverRemoteTarget);

        getServer().receiver.setTarget(applicationDeriveTarget(serverRemoteTarget));

        assertEndpointState(getServer().receiver, UNINITIALIZED, ACTIVE);
        getServer().receiver.open();

        assertEndpointState(getServer().receiver, ACTIVE, ACTIVE);

        pumpServerToClient();
        assertEndpointState(getClient().sender, ACTIVE, ACTIVE);

        getServer().receiver.flow(1);
        pumpServerToClient();


        LOGGER.fine(bold("======== About to create a message and send it to the server"));

        getClient().message = Proton.message();
        Section messageBody = new AmqpValue("Hello");
        getClient().message.setBody(messageBody);
        getClient().messageData = new byte[BUFFER_SIZE];
        int lengthOfEncodedMessage = getClient().message.encode(getClient().messageData, 0, BUFFER_SIZE);
        getTestLoggingHelper().prettyPrint(TestLoggingHelper.MESSAGE_PREFIX, Arrays.copyOf(getClient().messageData, lengthOfEncodedMessage));

        byte[] deliveryTag = "delivery1".getBytes();
        getClient().delivery = getClient().sender.delivery(deliveryTag);
        int numberOfBytesAcceptedBySender = getClient().sender.send(getClient().messageData, 0, lengthOfEncodedMessage);
        assertEquals("For simplicity, assume the sender can accept all the data",
                     lengthOfEncodedMessage, numberOfBytesAcceptedBySender);

        assertNull(getClient().delivery.getLocalState());

        boolean senderAdvanced = getClient().sender.advance();
        assertTrue("sender has not advanced", senderAdvanced);

        pumpClientToServer();


        LOGGER.fine(bold("======== About to process the message on the server"));

        getServer().delivery = getServer().connection.getWorkHead();
        assertEquals("The received delivery should be on our receiver",
                getServer().receiver, getServer().delivery.getLink());
        assertNull(getServer().delivery.getLocalState());
        assertNull(getServer().delivery.getRemoteState());

        assertFalse(getServer().delivery.isPartial());
        assertTrue(getServer().delivery.isReadable());

        getServer().messageData = new byte[BUFFER_SIZE];
        int numberOfBytesProducedByReceiver = getServer().receiver.recv(getServer().messageData, 0, BUFFER_SIZE);
        assertEquals(numberOfBytesAcceptedBySender, numberOfBytesProducedByReceiver);

        getServer().message = Proton.message();
        getServer().message.decode(getServer().messageData, 0, numberOfBytesProducedByReceiver);

        boolean messageProcessed = applicationProcessMessage(getServer().message);
        assertTrue(messageProcessed);

        getServer().delivery.disposition(Accepted.getInstance());
        assertEquals(Accepted.getInstance(), getServer().delivery.getLocalState());

        pumpServerToClient();
        assertEquals(Accepted.getInstance(), getClient().delivery.getRemoteState());


        LOGGER.fine(bold("======== About to accept and settle the message on the client"));

        Delivery clientDelivery = getClient().connection.getWorkHead();
        assertEquals(getClient().delivery, clientDelivery);
        assertTrue(clientDelivery.isUpdated());
        assertEquals(getClient().sender, clientDelivery.getLink());
        clientDelivery.disposition(clientDelivery.getRemoteState());
        assertEquals(Accepted.getInstance(), getClient().delivery.getLocalState());

        clientDelivery.settle();
        assertNull("Now we've settled, the delivery should no longer be in the work list", getClient().connection.getWorkHead());

        pumpClientToServer();


        LOGGER.fine(bold("======== About to settle the message on the server"));

        assertEquals(Accepted.getInstance(), getServer().delivery.getRemoteState());
        Delivery serverDelivery = getServer().connection.getWorkHead();
        assertEquals(getServer().delivery, serverDelivery);
        assertTrue(serverDelivery.isUpdated());
        assertTrue("Client should have already settled", serverDelivery.remotelySettled());
        serverDelivery.settle();
        assertTrue(serverDelivery.isSettled());
        assertNull("Now we've settled, the delivery should no longer be in the work list", getServer().connection.getWorkHead());

        // Increment the receiver's credit so its ready for another message.
        // When using proton-c, this call is required in order to generate a Flow frame
        // (proton-j sends one even without it to eagerly restore the session incoming window).
        getServer().receiver.flow(1);
        pumpServerToClient();


        LOGGER.fine(bold("======== About to close client's sender"));

        getClient().sender.close();

        pumpClientToServer();


        LOGGER.fine(bold("======== Server about to process client's link closure"));

        assertSame(getServer().receiver, getServer().connection.linkHead(of(ACTIVE), of(CLOSED)));
        getServer().receiver.close();

        pumpServerToClient();


        LOGGER.fine(bold("======== About to close client's session"));

        getClient().session.close();

        pumpClientToServer();


        LOGGER.fine(bold("======== Server about to process client's session closure"));

        assertSame(getServer().session, getServer().connection.sessionHead(of(ACTIVE), of(CLOSED)));
        getServer().session.close();

        pumpServerToClient();


        LOGGER.fine(bold("======== About to close client's connection"));

        getClient().connection.close();

        pumpClientToServer();


        LOGGER.fine(bold("======== Server about to process client's connection closure"));

        assertEquals(CLOSED, getServer().connection.getRemoteState());
        getServer().connection.close();

        pumpServerToClient();


        LOGGER.fine(bold("======== Checking client has nothing more to pump"));


        assertClientHasNothingToOutput();

        LOGGER.fine(bold("======== Done!"));
    }

    /**
     * Simulates creating a local terminus using the properties supplied by the remote link endpoint.
     *
     * In a broker you'd usually overlay serverRemoteTarget (eg its filter properties) onto
     * an existing object (which eg contains whether it's a queue or a topic), creating a new one from that
     * overlay. Also if this is link recovery then you'd fetch the unsettled map too.
     */
    private org.apache.qpid.proton.amqp.transport.Target applicationDeriveTarget(org.apache.qpid.proton.amqp.transport.Target serverRemoteTarget)
    {
        return serverRemoteTarget;
    }

    /**
     * Simulates processing a message.
     */
    private boolean applicationProcessMessage(Message message)
    {
        Object messageBody = ((AmqpValue)message.getBody()).getValue();
        return "Hello".equals(messageBody);
    }
}
