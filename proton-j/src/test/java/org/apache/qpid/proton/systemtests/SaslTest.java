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

import static org.apache.qpid.proton.systemtests.TestLoggingHelper.bold;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.fail;

import java.util.logging.Logger;

import org.apache.qpid.proton.Proton;
import org.apache.qpid.proton.engine.Sasl;
import org.junit.Test;

public class SaslTest extends EngineTestBase
{
    private static final Logger LOGGER = Logger.getLogger(SaslTest.class.getName());

    @Test
    public void testSaslHostnamePropagationAndRetrieval() throws Exception
    {
        LOGGER.fine(bold("======== About to create transports"));

        getClient().transport = Proton.transport();
        ProtocolTracerEnabler.setProtocolTracer(getClient().transport, TestLoggingHelper.CLIENT_PREFIX);

        Sasl clientSasl = getClient().transport.sasl();
        clientSasl.client();

        // Set the server hostname we are connecting to from the client
        String hostname = "my-remote-host-123";
        clientSasl.setRemoteHostname(hostname);

        // Verify we can't get the hostname on the client
        try
        {
            clientSasl.getHostname();
            fail("should have throw IllegalStateException");
        }
        catch (IllegalStateException ise)
        {
            // expected
        }

        getServer().transport = Proton.transport();
        ProtocolTracerEnabler.setProtocolTracer(getServer().transport, "            " + TestLoggingHelper.SERVER_PREFIX);

        // Configure the server to do ANONYMOUS
        Sasl serverSasl = getServer().transport.sasl();
        serverSasl.server();
        serverSasl.setMechanisms("ANONYMOUS");

        // Verify we can't set the hostname on the server
        try
        {
            serverSasl.setRemoteHostname("some-other-host");
            fail("should have throw IllegalStateException");
        }
        catch (IllegalStateException ise)
        {
            // expected
        }

        assertNull(serverSasl.getHostname());
        assertArrayEquals(new String[0], clientSasl.getRemoteMechanisms());

        pumpClientToServer();
        pumpServerToClient();

        // Verify we got the mechs, set the chosen mech, and verify the
        // server still doesnt know the hostname set/requested by the client
        assertArrayEquals(new String[] {"ANONYMOUS"} , clientSasl.getRemoteMechanisms());
        clientSasl.setMechanisms("ANONYMOUS");
        assertNull(serverSasl.getHostname());

        pumpClientToServer();

        // Verify the server now knows that the client set the hostname field
        assertEquals(hostname, serverSasl.getHostname());
    }

}
