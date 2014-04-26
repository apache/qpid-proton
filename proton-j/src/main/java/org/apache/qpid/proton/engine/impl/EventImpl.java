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

import org.apache.qpid.proton.engine.Event;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Session;
import org.apache.qpid.proton.engine.Link;
import org.apache.qpid.proton.engine.Delivery;
import org.apache.qpid.proton.engine.Transport;

/**
 * EventImpl
 *
 */

class EventImpl implements Event
{

    Type type;
    Connection connection;
    Session session;
    Link link;
    Delivery delivery;
    Transport transport;

    EventImpl(Type type)
    {
        this.type = type;
    }

    public Category getCategory()
    {
        return type.getCategory();
    }

    public Type getType()
    {
        return type;
    }

    public Connection getConnection()
    {
        return connection;
    }

    public Session getSession()
    {
        return session;
    }

    public Link getLink()
    {
        return link;
    }

    public Delivery getDelivery()
    {
        return delivery;
    }

    public Transport getTransport()
    {
        return transport;
    }

    void init(Transport transport)
    {
        this.transport = transport;
    }

    void init(Connection connection)
    {
        this.connection = connection;
        init(((ConnectionImpl) connection).getTransport());
    }

    void init(Session session)
    {
        this.session = session;
        init(session.getConnection());
    }

    void init(Link link)
    {
        this.link = link;
        init(link.getSession());
    }

    void init(Delivery delivery)
    {
        this.delivery = delivery;
        init(delivery.getLink());
    }

}
