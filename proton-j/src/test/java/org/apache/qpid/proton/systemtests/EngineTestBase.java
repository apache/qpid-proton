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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.nio.ByteBuffer;
import java.util.logging.Logger;

import org.apache.qpid.proton.amqp.messaging.Target;
import org.apache.qpid.proton.engine.Endpoint;
import org.apache.qpid.proton.engine.EndpointState;

public abstract class EngineTestBase
{
    private static final Logger LOGGER = Logger.getLogger(EngineTestBase.class.getName());

    private final TestLoggingHelper _testLoggingHelper = new TestLoggingHelper(LOGGER);
    private final ProtonContainer _client = new ProtonContainer("clientContainer");
    private final ProtonContainer _server = new ProtonContainer("serverContainer");

    protected TestLoggingHelper getTestLoggingHelper()
    {
        return _testLoggingHelper;
    }

    protected ProtonContainer getClient()
    {
        return _client;
    }

    protected ProtonContainer getServer()
    {
        return _server;
    }

    protected void assertClientHasNothingToOutput()
    {
        assertEquals(0, getClient().transport.getOutputBuffer().remaining());
        getClient().transport.outputConsumed();
    }

    protected void pumpServerToClient()
    {
        ByteBuffer serverBuffer = getServer().transport.getOutputBuffer();

        getTestLoggingHelper().prettyPrint("          <<<" + TestLoggingHelper.SERVER_PREFIX + " ", serverBuffer);
        assertTrue("Server expected to produce some output", serverBuffer.hasRemaining());

        ByteBuffer clientBuffer = getClient().transport.getInputBuffer();

        clientBuffer.put(serverBuffer);

        assertEquals("Client expected to consume all server's output", 0, serverBuffer.remaining());

        getClient().transport.processInput().checkIsOk();
        getServer().transport.outputConsumed();
    }

    protected void pumpClientToServer()
    {
        ByteBuffer clientBuffer = getClient().transport.getOutputBuffer();

        getTestLoggingHelper().prettyPrint(TestLoggingHelper.CLIENT_PREFIX + ">>> ", clientBuffer);
        assertTrue("Client expected to produce some output", clientBuffer.hasRemaining());

        ByteBuffer serverBuffer = getServer().transport.getInputBuffer();

        serverBuffer.put(clientBuffer);

        assertEquals("Server expected to consume all client's output", 0, clientBuffer.remaining());

        getClient().transport.outputConsumed();
        getServer().transport.processInput().checkIsOk();
    }

    protected void doOutputInputCycle() throws Exception
    {
        pumpClientToServer();

        pumpServerToClient();
    }

    protected void assertEndpointState(Endpoint endpoint, EndpointState localState, EndpointState remoteState)
    {
        assertEquals(localState, endpoint.getLocalState());
        assertEquals(remoteState, endpoint.getRemoteState());
    }

    protected void assertTerminusEquals(org.apache.qpid.proton.amqp.transport.Target expectedTarget, org.apache.qpid.proton.amqp.transport.Target actualTarget)
    {
        assertEquals(
                ((Target)expectedTarget).getAddress(),
                ((Target)actualTarget).getAddress());
    }
}
