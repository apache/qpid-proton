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
    Object context;
    EventImpl next;

    EventImpl()
    {
        this.type = null;
    }

    void init(Event.Type type, Object context)
    {
        this.type = type;
        this.context = context;
    }

    void clear()
    {
        type = null;
        context = null;
    }

    public Type getType()
    {
        return type;
    }

    public Object getContext()
    {
        return context;
    }

    public Connection getConnection()
    {
        if (context instanceof Connection) {
            return (Connection) context;
        } else if (context instanceof Transport) {
            Transport transport = getTransport();
            if (transport == null) {
                return null;
            }
            return ((TransportImpl) transport).getConnectionImpl();
        } else {
            Session ssn = getSession();
            if (ssn == null) {
                return null;
            }
            return ssn.getConnection();
        }
    }

    public Session getSession()
    {
        if (context instanceof Session) {
            return (Session) context;
        } else {
            Link link = getLink();
            if (link == null) {
                return null;
            }
            return link.getSession();
        }
    }

    public Link getLink()
    {
        if (context instanceof Link) {
            return (Link) context;
        } else {
            Delivery dlv = getDelivery();
            if (dlv == null) {
                return null;
            }
            return dlv.getLink();
        }
    }

    public Delivery getDelivery()
    {
        if (context instanceof Delivery) {
            return (Delivery) context;
        } else {
            return null;
        }
    }

    public Transport getTransport()
    {
        if (context instanceof Transport) {
            return (Transport) context;
        } else {
            return null;
        }
    }
    public Event copy()
    {
       EventImpl newEvent = new EventImpl();
       newEvent.init(type, context);
       return newEvent;
    }

    @Override
    public String toString()
    {
        return "EventImpl{" + "type=" + type + ", context=" + context + '}';
    }

}
