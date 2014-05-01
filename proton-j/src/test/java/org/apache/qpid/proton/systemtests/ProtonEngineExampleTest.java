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

import java.nio.ByteBuffer;
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
import org.apache.qpid.proton.engine.Endpoint;
import org.apache.qpid.proton.engine.EndpointState;
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
public class ProtonEngineExampleTest
{
    private static final Logger LOGGER = Logger.getLogger(ProtonEngineExampleTest.class.getName());

    private static final int BUFFER_SIZE = 4096;

    private TestLoggingHelper _testLoggingHelper = new TestLoggingHelper(LOGGER);

    private final ProtonContainer _client = new ProtonContainer("clientContainer");
    private final ProtonContainer _server = new ProtonContainer("serverContainer");

    private final String _targetAddress = _server.containerId + "-link1-target";

    @Test
    public void test() throws Exception
    {
        LOGGER.fine(bold("======== About to create transports"));

        _client.transport = Proton.transport();
        ProtocolTracerEnabler.setProtocolTracer(_client.transport, TestLoggingHelper.CLIENT_PREFIX);

        _server.transport = Proton.transport();
        ProtocolTracerEnabler.setProtocolTracer(_server.transport, "            " + TestLoggingHelper.SERVER_PREFIX);

        doOutputInputCycle();

        _client.connection = Proton.connection();
        _client.transport.bind(_client.connection);

        _server.connection = Proton.connection();
        _server.transport.bind(_server.connection);



        LOGGER.fine(bold("======== About to open connections"));
        _client.connection.open();
        _server.connection.open();

        doOutputInputCycle();



        LOGGER.fine(bold("======== About to open sessions"));
        _client.session = _client.connection.session();
        _client.session.open();

        pumpClientToServer();

        _server.session = _server.connection.sessionHead(of(UNINITIALIZED), of(ACTIVE));
        assertEndpointState(_server.session, UNINITIALIZED, ACTIVE);

        _server.session.open();
        assertEndpointState(_server.session, ACTIVE, ACTIVE);

        pumpServerToClient();
        assertEndpointState(_client.session, ACTIVE, ACTIVE);



        LOGGER.fine(bold("======== About to create sender"));

        _client.source = new Source();
        _client.source.setAddress(null);

        _client.target = new Target();
        _client.target.setAddress(_targetAddress);

        _client.sender = _client.session.sender("link1");
        _client.sender.setTarget(_client.target);
        _client.sender.setSource(_client.source);
        // Exactly once delivery semantics
        _client.sender.setSenderSettleMode(SenderSettleMode.UNSETTLED);
        _client.sender.setReceiverSettleMode(ReceiverSettleMode.SECOND);

        assertEndpointState(_client.sender, UNINITIALIZED, UNINITIALIZED);

        _client.sender.open();
        assertEndpointState(_client.sender, ACTIVE, UNINITIALIZED);

        pumpClientToServer();



        LOGGER.fine(bold("======== About to set up implicitly created receiver"));

        // A real application would be interested in more states than simply ACTIVE, as there
        // exists the possibility that the link could have moved to another state already e.g. CLOSED.
        // (See pipelining).
        _server.receiver = (Receiver) _server.connection.linkHead(of(UNINITIALIZED), of(ACTIVE));
        // Accept the settlement modes suggested by the client
        _server.receiver.setSenderSettleMode(_server.receiver.getRemoteSenderSettleMode());
        _server.receiver.setReceiverSettleMode(_server.receiver.getRemoteReceiverSettleMode());

        org.apache.qpid.proton.amqp.transport.Target serverRemoteTarget = _server.receiver.getRemoteTarget();
        assertTerminusEquals(_client.target, serverRemoteTarget);

        _server.receiver.setTarget(applicationDeriveTarget(serverRemoteTarget));

        assertEndpointState(_server.receiver, UNINITIALIZED, ACTIVE);
        _server.receiver.open();

        assertEndpointState(_server.receiver, ACTIVE, ACTIVE);

        pumpServerToClient();
        assertEndpointState(_client.sender, ACTIVE, ACTIVE);

        _server.receiver.flow(1);
        pumpServerToClient();


        LOGGER.fine(bold("======== About to create a message and send it to the server"));

        _client.message = Proton.message();
        Section messageBody = new AmqpValue("Hello");
        _client.message.setBody(messageBody);
        _client.messageData = new byte[BUFFER_SIZE];
        int lengthOfEncodedMessage = _client.message.encode(_client.messageData, 0, BUFFER_SIZE);
        _testLoggingHelper.prettyPrint(TestLoggingHelper.MESSAGE_PREFIX, Arrays.copyOf(_client.messageData, lengthOfEncodedMessage));

        byte[] deliveryTag = "delivery1".getBytes();
        _client.delivery = _client.sender.delivery(deliveryTag);
        int numberOfBytesAcceptedBySender = _client.sender.send(_client.messageData, 0, lengthOfEncodedMessage);
        assertEquals("For simplicity, assume the sender can accept all the data",
                     lengthOfEncodedMessage, numberOfBytesAcceptedBySender);

        assertNull(_client.delivery.getLocalState());

        boolean senderAdvanced = _client.sender.advance();
        assertTrue("sender has not advanced", senderAdvanced);

        pumpClientToServer();


        LOGGER.fine(bold("======== About to process the message on the server"));

        _server.delivery = _server.connection.getWorkHead();
        assertEquals("The received delivery should be on our receiver",
                _server.receiver, _server.delivery.getLink());
        assertNull(_server.delivery.getLocalState());
        assertNull(_server.delivery.getRemoteState());

        assertFalse(_server.delivery.isPartial());
        assertTrue(_server.delivery.isReadable());

        _server.messageData = new byte[BUFFER_SIZE];
        int numberOfBytesProducedByReceiver = _server.receiver.recv(_server.messageData, 0, BUFFER_SIZE);
        assertEquals(numberOfBytesAcceptedBySender, numberOfBytesProducedByReceiver);

        _server.message = Proton.message();
        _server.message.decode(_server.messageData, 0, numberOfBytesProducedByReceiver);

        boolean messageProcessed = applicationProcessMessage(_server.message);
        assertTrue(messageProcessed);

        _server.delivery.disposition(Accepted.getInstance());
        assertEquals(Accepted.getInstance(), _server.delivery.getLocalState());

        pumpServerToClient();
        assertEquals(Accepted.getInstance(), _client.delivery.getRemoteState());


        LOGGER.fine(bold("======== About to accept and settle the message on the client"));

        Delivery clientDelivery = _client.connection.getWorkHead();
        assertEquals(_client.delivery, clientDelivery);
        assertTrue(clientDelivery.isUpdated());
        assertEquals(_client.sender, clientDelivery.getLink());
        clientDelivery.disposition(clientDelivery.getRemoteState());
        assertEquals(Accepted.getInstance(), _client.delivery.getLocalState());

        clientDelivery.settle();
        assertNull("Now we've settled, the delivery should no longer be in the work list", _client.connection.getWorkHead());

        pumpClientToServer();


        LOGGER.fine(bold("======== About to settle the message on the server"));

        assertEquals(Accepted.getInstance(), _server.delivery.getRemoteState());
        Delivery serverDelivery = _server.connection.getWorkHead();
        assertEquals(_server.delivery, serverDelivery);
        assertTrue(serverDelivery.isUpdated());
        assertTrue("Client should have already settled", serverDelivery.remotelySettled());
        serverDelivery.settle();
        assertTrue(serverDelivery.isSettled());
        assertNull("Now we've settled, the delivery should no longer be in the work list", _server.connection.getWorkHead());

        // Increment the receiver's credit so its ready for another message.
        // When using proton-c, this call is required in order to generate a Flow frame
        // (proton-j sends one even without it to eagerly restore the session incoming window).
        _server.receiver.flow(1);
        pumpServerToClient();


        LOGGER.fine(bold("======== About to close client's sender"));

        _client.sender.close();

        pumpClientToServer();


        LOGGER.fine(bold("======== Server about to process client's link closure"));

        assertSame(_server.receiver, _server.connection.linkHead(of(ACTIVE), of(CLOSED)));
        _server.receiver.close();

        pumpServerToClient();


        LOGGER.fine(bold("======== About to close client's session"));

        _client.session.close();

        pumpClientToServer();


        LOGGER.fine(bold("======== Server about to process client's session closure"));

        assertSame(_server.session, _server.connection.sessionHead(of(ACTIVE), of(CLOSED)));
        _server.session.close();

        pumpServerToClient();


        LOGGER.fine(bold("======== About to close client's connection"));

        _client.connection.close();

        pumpClientToServer();


        LOGGER.fine(bold("======== Server about to process client's connection closure"));

        assertEquals(CLOSED, _server.connection.getRemoteState());
        _server.connection.close();

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

    private void assertTerminusEquals(
            org.apache.qpid.proton.amqp.transport.Target expectedTarget,
            org.apache.qpid.proton.amqp.transport.Target actualTarget)
    {
        assertEquals(
                ((Target)expectedTarget).getAddress(),
                ((Target)actualTarget).getAddress());
    }

    private void assertEndpointState(Endpoint endpoint, EndpointState localState, EndpointState remoteState)
    {
        assertEquals(localState, endpoint.getLocalState());
        assertEquals(remoteState, endpoint.getRemoteState());
    }

    private void doOutputInputCycle() throws Exception
    {
        pumpClientToServer();

        pumpServerToClient();
    }

    private void pumpClientToServer()
    {
        ByteBuffer clientBuffer = _client.transport.getOutputBuffer();

        _testLoggingHelper.prettyPrint(TestLoggingHelper.CLIENT_PREFIX + ">>> ", clientBuffer);
        assertTrue("Client expected to produce some output", clientBuffer.hasRemaining());

        ByteBuffer serverBuffer = _server.transport.getInputBuffer();

        serverBuffer.put(clientBuffer);

        assertEquals("Server expected to consume all client's output", 0, clientBuffer.remaining());

        _client.transport.outputConsumed();
        _server.transport.processInput().checkIsOk();
    }

    private void pumpServerToClient()
    {
        ByteBuffer serverBuffer = _server.transport.getOutputBuffer();

        _testLoggingHelper.prettyPrint("          <<<" + TestLoggingHelper.SERVER_PREFIX + " ", serverBuffer);
        assertTrue("Server expected to produce some output", serverBuffer.hasRemaining());

        ByteBuffer clientBuffer = _client.transport.getInputBuffer();

        clientBuffer.put(serverBuffer);

        assertEquals("Client expected to consume all server's output", 0, serverBuffer.remaining());

        _client.transport.processInput().checkIsOk();
        _server.transport.outputConsumed();
    }

    private void assertClientHasNothingToOutput()
    {
        assertEquals(0, _client.transport.getOutputBuffer().remaining());
        _client.transport.outputConsumed();
    }
}
