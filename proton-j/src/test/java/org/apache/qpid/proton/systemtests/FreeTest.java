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
import static org.apache.qpid.proton.engine.EndpointState.UNINITIALIZED;
import static org.apache.qpid.proton.systemtests.TestLoggingHelper.bold;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;

import java.util.logging.Logger;

import org.apache.qpid.proton.Proton;
import org.apache.qpid.proton.amqp.messaging.Source;
import org.apache.qpid.proton.amqp.messaging.Target;
import org.apache.qpid.proton.amqp.transport.ReceiverSettleMode;
import org.apache.qpid.proton.amqp.transport.SenderSettleMode;
import org.apache.qpid.proton.engine.Receiver;
import org.apache.qpid.proton.engine.Sender;
import org.apache.qpid.proton.engine.Session;
import org.junit.Test;

public class FreeTest extends EngineTestBase
{
    private static final Logger LOGGER = Logger.getLogger(FreeTest.class.getName());

    @Test
    public void testFreeConnectionWithMultipleSessionsAndSendersAndReceiversDoesNotThrowCME() throws Exception
    {
        LOGGER.fine(bold("======== About to create transports"));

        getClient().transport = Proton.transport();
        ProtocolTracerEnabler.setProtocolTracer(getClient().transport, TestLoggingHelper.CLIENT_PREFIX);

        getServer().transport = Proton.transport();
        ProtocolTracerEnabler.setProtocolTracer(getServer().transport, "            " + TestLoggingHelper.SERVER_PREFIX);

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

        Session clientSession2 = getClient().connection.session();
        clientSession2.open();

        pumpClientToServer();

        getServer().session = getServer().connection.sessionHead(of(UNINITIALIZED), of(ACTIVE));
        assertEndpointState(getServer().session, UNINITIALIZED, ACTIVE);

        getServer().session.open();
        assertEndpointState(getServer().session, ACTIVE, ACTIVE);

        Session serverSession2 = getServer().connection.sessionHead(of(UNINITIALIZED), of(ACTIVE));
        assertNotNull("Engine did not return expected second server session", serverSession2);
        assertNotSame("Engine did not return expected second server session", serverSession2, getServer().session);
        serverSession2.open();

        pumpServerToClient();
        assertEndpointState(getClient().session, ACTIVE, ACTIVE);
        assertEndpointState(clientSession2, ACTIVE, ACTIVE);



        LOGGER.fine(bold("======== About to create client senders"));

        getClient().source = new Source();
        getClient().source.setAddress(null);

        getClient().target = new Target();
        getClient().target.setAddress("myQueue");

        getClient().sender = getClient().session.sender("sender1");
        getClient().sender.setTarget(getClient().target);
        getClient().sender.setSource(getClient().source);

        getClient().sender.setSenderSettleMode(SenderSettleMode.UNSETTLED);
        getClient().sender.setReceiverSettleMode(ReceiverSettleMode.FIRST);

        assertEndpointState(getClient().sender, UNINITIALIZED, UNINITIALIZED);

        getClient().sender.open();
        assertEndpointState(getClient().sender, ACTIVE, UNINITIALIZED);


        Sender clientSender2 = getClient().session.sender("sender2");
        clientSender2.setTarget(getClient().target);
        clientSender2.setSource(getClient().source);

        clientSender2.setSenderSettleMode(SenderSettleMode.UNSETTLED);
        clientSender2.setReceiverSettleMode(ReceiverSettleMode.FIRST);

        assertEndpointState(clientSender2, UNINITIALIZED, UNINITIALIZED);

        clientSender2.open();
        assertEndpointState(clientSender2, ACTIVE, UNINITIALIZED);

        pumpClientToServer();


        LOGGER.fine(bold("======== About to set up server receivers"));

        getServer().receiver = (Receiver) getServer().connection.linkHead(of(UNINITIALIZED), of(ACTIVE));
        // Accept the settlement modes suggested by the client
        getServer().receiver.setSenderSettleMode(getServer().receiver.getRemoteSenderSettleMode());
        getServer().receiver.setReceiverSettleMode(getServer().receiver.getRemoteReceiverSettleMode());

        org.apache.qpid.proton.amqp.transport.Target serverRemoteTarget = getServer().receiver.getRemoteTarget();
        assertTerminusEquals(getClient().target, serverRemoteTarget);

        getServer().receiver.setTarget(serverRemoteTarget);

        assertEndpointState(getServer().receiver, UNINITIALIZED, ACTIVE);
        getServer().receiver.open();

        assertEndpointState(getServer().receiver, ACTIVE, ACTIVE);

        Receiver serverReceiver2 = (Receiver) getServer().connection.linkHead(of(UNINITIALIZED), of(ACTIVE));
        serverReceiver2.open();
        assertEndpointState(serverReceiver2, ACTIVE, ACTIVE);

        pumpServerToClient();
        assertEndpointState(getClient().sender, ACTIVE, ACTIVE);
        assertEndpointState(clientSender2, ACTIVE, ACTIVE);



        LOGGER.fine(bold("======== About to create client receivers"));

        Source src = new Source();
        src.setAddress("myQueue");

        Target tgt1 = new Target();
        tgt1.setAddress("receiver1");

        getClient().receiver = getClient().session.receiver("receiver1");
        getClient().receiver.setSource(src);
        getClient().receiver.setTarget(tgt1);

        getClient().receiver.setSenderSettleMode(SenderSettleMode.UNSETTLED);
        getClient().receiver.setReceiverSettleMode(ReceiverSettleMode.FIRST);

        assertEndpointState(getClient().receiver, UNINITIALIZED, UNINITIALIZED);

        getClient().receiver.open();
        assertEndpointState(getClient().receiver, ACTIVE, UNINITIALIZED);


        Target tgt2 = new Target();
        tgt1.setAddress("receiver2");

        Receiver clientReceiver2 = getClient().session.receiver("receiver2");
        clientReceiver2.setSource(src);
        clientReceiver2.setTarget(tgt2);

        clientReceiver2.setSenderSettleMode(SenderSettleMode.UNSETTLED);
        clientReceiver2.setReceiverSettleMode(ReceiverSettleMode.FIRST);

        assertEndpointState(clientReceiver2, UNINITIALIZED, UNINITIALIZED);

        clientReceiver2.open();
        assertEndpointState(clientReceiver2, ACTIVE, UNINITIALIZED);

        pumpClientToServer();



        LOGGER.fine(bold("======== About to set up server senders"));

        getServer().sender = (Sender) getServer().connection.linkHead(of(UNINITIALIZED), of(ACTIVE));
        // Accept the settlement modes suggested by the client
        getServer().sender.setSenderSettleMode(getServer().sender.getRemoteSenderSettleMode());
        getServer().sender.setReceiverSettleMode(getServer().sender.getRemoteReceiverSettleMode());

        org.apache.qpid.proton.amqp.transport.Target serverRemoteTarget2 = getServer().sender.getRemoteTarget();
        assertTerminusEquals(tgt1, serverRemoteTarget2);

        getServer().sender.setTarget(serverRemoteTarget2);

        assertEndpointState(getServer().sender, UNINITIALIZED, ACTIVE);
        getServer().sender.open();
        assertEndpointState(getServer().sender, ACTIVE, ACTIVE);

        Sender serverSender2 = (Sender) getServer().connection.linkHead(of(UNINITIALIZED), of(ACTIVE));

        serverRemoteTarget2 = serverSender2.getRemoteTarget();
        assertTerminusEquals(tgt2, serverRemoteTarget2);
        serverSender2.setTarget(serverRemoteTarget2);
        serverSender2.open();
        assertEndpointState(serverSender2, ACTIVE, ACTIVE);

        pumpServerToClient();
        assertEndpointState(getClient().receiver, ACTIVE, ACTIVE);
        assertEndpointState(clientReceiver2, ACTIVE, ACTIVE);



        LOGGER.fine(bold("======== About to close and free client's connection"));

        getClient().connection.close();
        getClient().connection.free();
    }

}
