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

    public Category getCategory()
    {
        return type.getCategory();
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
        switch (type) {
        case CONNECTION_REMOTE_STATE:
        case CONNECTION_LOCAL_STATE:
        case CONNECTION_INIT:
        case CONNECTION_FINAL:
            return (Connection) context;
        case TRANSPORT:
            Transport transport = getTransport();
            if (transport == null) {
                return null;
            }
            return ((TransportImpl) transport).getConnectionImpl();
        default:
            Session ssn = getSession();
            if (ssn == null) {
                return null;
            }
            return ssn.getConnection();
        }
    }

    public Session getSession()
    {
        switch (type) {
        case SESSION_REMOTE_STATE:
        case SESSION_LOCAL_STATE:
        case SESSION_INIT:
        case SESSION_FINAL:
            return (Session) context;
        default:
            Link link = getLink();
            if (link == null) {
                return null;
            }
            return link.getSession();
        }
    }

    public Link getLink()
    {
        switch (type) {
        case LINK_REMOTE_STATE:
        case LINK_LOCAL_STATE:
        case LINK_INIT:
        case LINK_FINAL:
        case LINK_FLOW:
            return (Link) context;
        default:
            Delivery dlv = getDelivery();
            if (dlv == null) {
                return null;
            }
            return dlv.getLink();
        }
    }

    public Delivery getDelivery()
    {
        switch (type) {
        case DELIVERY:
            return (Delivery) context;
        default:
            return null;
        }
    }

    public Transport getTransport()
    {
        switch (type) {
        case TRANSPORT:
            return (Transport) context;
        default:
            return null;
        }
    }

    @Override
    public String toString()
    {
        return "EventImpl{" + "type=" + type + ", context=" + context + '}';
    }
}
