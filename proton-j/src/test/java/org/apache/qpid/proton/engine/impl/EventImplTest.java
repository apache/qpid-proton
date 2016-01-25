/*
 *
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
 *
 */
package org.apache.qpid.proton.engine.impl;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertSame;

import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Delivery;
import org.apache.qpid.proton.engine.Event;
import org.apache.qpid.proton.engine.EventType;
import org.apache.qpid.proton.engine.Link;
import org.apache.qpid.proton.engine.Sender;
import org.apache.qpid.proton.engine.Session;
import org.apache.qpid.proton.engine.Transport;
import org.junit.Test;

public class EventImplTest
{
    @Test
    public void testGetTransportWithConnectionContext()
    {
        Transport transport = Transport.Factory.create();
        Connection connection = Connection.Factory.create();
        transport.bind(connection);

        EventImpl event = createEvent(connection, Event.Type.CONNECTION_BOUND);

        assertNotNull("No transport returned", event.getTransport());
        assertSame("Incorrect transport returned", transport, event.getTransport());
    }

    @Test
    public void testGetTransportWithTransportContext()
    {
        Transport transport = Transport.Factory.create();
        Connection connection = Connection.Factory.create();
        transport.bind(connection);

        EventImpl event = createEvent(transport, Event.Type.TRANSPORT);

        assertNotNull("No transport returned", event.getTransport());
        assertSame("Incorrect transport returned", transport, event.getTransport());
    }

    @Test
    public void testGetTransportWithSessionContext()
    {
        Transport transport = Transport.Factory.create();
        Connection connection = Connection.Factory.create();
        transport.bind(connection);

        Session session = connection.session();

        EventImpl event = createEvent(session, Event.Type.SESSION_INIT);

        assertNotNull("No transport returned", event.getTransport());
        assertSame("Incorrect transport returned", transport, event.getTransport());
    }

    @Test
    public void testGetTransportWithLinkContext()
    {
        Transport transport = Transport.Factory.create();
        Connection connection = Connection.Factory.create();
        transport.bind(connection);

        Session session = connection.session();
        Link link = session.receiver("myReceiver");

        EventImpl event = createEvent(link, Event.Type.LINK_INIT);

        assertNotNull("No transport returned", event.getTransport());
        assertSame("Incorrect transport returned", transport, event.getTransport());
    }

    @Test
    public void testGetTransportWithDeliveryContext()
    {
        Transport transport = Transport.Factory.create();
        Connection connection = Connection.Factory.create();
        transport.bind(connection);

        Session session = connection.session();
        Sender sender = session.sender("mySender");

        Delivery delivery = sender.delivery("tag".getBytes());

        EventImpl event = createEvent(delivery, Event.Type.DELIVERY);

        assertNotNull("No transport returned", event.getTransport());
        assertSame("Incorrect transport returned", transport, event.getTransport());
    }

    EventImpl createEvent(Object context, EventType type)
    {
        EventImpl event = new EventImpl();
        event.init(type, context);
        return event;
    }
}
