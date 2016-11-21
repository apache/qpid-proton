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
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.logging.Logger;

import org.apache.qpid.proton.Proton;
import org.apache.qpid.proton.amqp.Symbol;
import org.junit.Test;

public class SessionTest extends EngineTestBase
{
    private static final Logger LOGGER = Logger.getLogger(SessionTest.class.getName());

    @Test
    public void testCapabilities() throws Exception
    {
        final Symbol clientOfferedCap = Symbol.valueOf("clientOfferedCapability");
        final Symbol clientDesiredCap = Symbol.valueOf("clientDesiredCapability");
        final Symbol serverOfferedCap = Symbol.valueOf("serverOfferedCapability");
        final Symbol serverDesiredCap = Symbol.valueOf("serverDesiredCapability");

        Symbol[] clientOfferedCapabilities = new Symbol[] { clientOfferedCap };
        Symbol[] clientDesiredCapabilities = new Symbol[] { clientDesiredCap };

        Symbol[] serverOfferedCapabilities = new Symbol[] { serverOfferedCap };
        Symbol[] serverDesiredCapabilities = new Symbol[] { serverDesiredCap };

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

        // Set the client session capabilities
        getClient().session.setOfferedCapabilities(clientOfferedCapabilities);
        getClient().session.setDesiredCapabilities(clientDesiredCapabilities);

        getClient().session.open();

        pumpClientToServer();

        getServer().session = getServer().connection.sessionHead(of(UNINITIALIZED), of(ACTIVE));
        assertEndpointState(getServer().session, UNINITIALIZED, ACTIVE);

        // Set the server session capabilities
        getServer().session.setOfferedCapabilities(serverOfferedCapabilities);
        getServer().session.setDesiredCapabilities(serverDesiredCapabilities);

        getServer().session.open();
        assertEndpointState(getServer().session, ACTIVE, ACTIVE);

        pumpServerToClient();

        // Verify server side got the clients session capabilities as expected
        Symbol[] serverRemoteOfferedCapabilities = getServer().session.getRemoteOfferedCapabilities();
        assertNotNull("Server had no remote offered capabilities", serverRemoteOfferedCapabilities);
        assertEquals("Server remote offered capabilities not expected size", 1, serverRemoteOfferedCapabilities.length);
        assertTrue("Server remote offered capabilities lack expected value: " + clientOfferedCap, Arrays.asList(serverRemoteOfferedCapabilities).contains(clientOfferedCap));

        Symbol[] serverRemoteDesiredCapabilities = getServer().session.getRemoteDesiredCapabilities();
        assertNotNull("Server had no remote desired capabilities", serverRemoteDesiredCapabilities);
        assertEquals("Server remote desired capabilities not expected size", 1, serverRemoteDesiredCapabilities.length);
        assertTrue("Server remote desired capabilities lack expected value: " + clientDesiredCap, Arrays.asList(serverRemoteDesiredCapabilities).contains(clientDesiredCap));

        // Verify the client side got the servers session capabilities as expected
        Symbol[]  clientRemoteOfferedCapabilities = getClient().session.getRemoteOfferedCapabilities();
        assertNotNull("Client had no remote offered capabilities", clientRemoteOfferedCapabilities);
        assertEquals("Client remote offered capabilities not expected size", 1, clientRemoteOfferedCapabilities.length);
        assertTrue("Client remote offered capabilities lack expected value: " + serverOfferedCap, Arrays.asList(clientRemoteOfferedCapabilities).contains(serverOfferedCap));

        Symbol[]  clientRemoteDesiredCapabilities = getClient().session.getRemoteDesiredCapabilities();
        assertNotNull("Client had no remote desired capabilities", clientRemoteDesiredCapabilities);
        assertEquals("Client remote desired capabilities not expected size", 1, clientRemoteDesiredCapabilities.length);
        assertTrue("Client remote desired capabilities lack expected value: " + serverDesiredCap, Arrays.asList(clientRemoteDesiredCapabilities).contains(serverDesiredCap));
    }

    @Test
    public void testProperties() throws Exception
    {
        final Symbol clientPropName = Symbol.valueOf("ClientPropName");
        final Integer clientPropValue = 1234;
        final Symbol serverPropName = Symbol.valueOf("ServerPropName");
        final Integer serverPropValue = 5678;

        Map<Symbol, Object> clientProps = new HashMap<>();
        clientProps.put(clientPropName, clientPropValue);

        Map<Symbol, Object> serverProps = new HashMap<>();
        serverProps.put(serverPropName, serverPropValue);

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

        // Set the client session properties
        getClient().session.setProperties(clientProps);

        getClient().session.open();

        pumpClientToServer();

        getServer().session = getServer().connection.sessionHead(of(UNINITIALIZED), of(ACTIVE));
        assertEndpointState(getServer().session, UNINITIALIZED, ACTIVE);

        // Set the server session properties
        getServer().session.setProperties(serverProps);

        getServer().session.open();

        assertEndpointState(getServer().session, ACTIVE, ACTIVE);

        pumpServerToClient();

        assertEndpointState(getClient().session, ACTIVE, ACTIVE);

        // Verify server side got the clients session properties as expected
        Map<Symbol, Object> serverRemoteProperties = getServer().session.getRemoteProperties();
        assertNotNull("Server had no remote properties", serverRemoteProperties);
        assertEquals("Server remote properties not expected size", 1, serverRemoteProperties.size());
        assertTrue("Server remote properties lack expected key: " + clientPropName, serverRemoteProperties.containsKey(clientPropName));
        assertEquals("Server remote properties contain unexpected value for key: " + clientPropName, clientPropValue, serverRemoteProperties.get(clientPropName));

        // Verify the client side got the servers session properties as expected
        Map<Symbol, Object> clientRemoteProperties = getClient().session.getRemoteProperties();
        assertNotNull("Client had no remote properties", clientRemoteProperties);
        assertEquals("Client remote properties not expected size", 1, clientRemoteProperties.size());
        assertTrue("Client remote properties lack expected key: " + serverPropName, clientRemoteProperties.containsKey(serverPropName));
        assertEquals("Client remote properties contain unexpected value for key: " + serverPropName, serverPropValue, clientRemoteProperties.get(serverPropName));
    }
}