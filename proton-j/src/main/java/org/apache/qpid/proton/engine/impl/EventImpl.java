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

import java.util.Iterator;

import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Delivery;
import org.apache.qpid.proton.engine.Event;
import org.apache.qpid.proton.engine.Handler;
import org.apache.qpid.proton.engine.Link;
import org.apache.qpid.proton.engine.Session;
import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.reactor.Reactor;
import org.apache.qpid.proton.reactor.Selectable;
import org.apache.qpid.proton.reactor.Task;

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

    @Override
    public Type getType()
    {
        return type;
    }

    @Override
    public Object getContext()
    {
        return context;
    }

    @Override
    public void dispatch(Handler handler)
    {
        switch (type) {
        case CONNECTION_INIT:
            handler.onConnectionInit(this);
            break;
        case CONNECTION_LOCAL_OPEN:
            handler.onConnectionLocalOpen(this);
            break;
        case CONNECTION_REMOTE_OPEN:
            handler.onConnectionRemoteOpen(this);
            break;
        case CONNECTION_LOCAL_CLOSE:
            handler.onConnectionLocalClose(this);
            break;
        case CONNECTION_REMOTE_CLOSE:
            handler.onConnectionRemoteClose(this);
            break;
        case CONNECTION_BOUND:
            handler.onConnectionBound(this);
            break;
        case CONNECTION_UNBOUND:
            handler.onConnectionUnbound(this);
            break;
        case CONNECTION_FINAL:
            handler.onConnectionFinal(this);
            break;
        case SESSION_INIT:
            handler.onSessionInit(this);
            break;
        case SESSION_LOCAL_OPEN:
            handler.onSessionLocalOpen(this);
            break;
        case SESSION_REMOTE_OPEN:
            handler.onSessionRemoteOpen(this);
            break;
        case SESSION_LOCAL_CLOSE:
            handler.onSessionLocalClose(this);
            break;
        case SESSION_REMOTE_CLOSE:
            handler.onSessionRemoteClose(this);
            break;
        case SESSION_FINAL:
            handler.onSessionFinal(this);
            break;
        case LINK_INIT:
            handler.onLinkInit(this);
            break;
        case LINK_LOCAL_OPEN:
            handler.onLinkLocalOpen(this);
            break;
        case LINK_REMOTE_OPEN:
            handler.onLinkRemoteOpen(this);
            break;
        case LINK_LOCAL_DETACH:
            handler.onLinkLocalDetach(this);
            break;
        case LINK_REMOTE_DETACH:
            handler.onLinkRemoteDetach(this);
            break;
        case LINK_LOCAL_CLOSE:
            handler.onLinkLocalClose(this);
            break;
        case LINK_REMOTE_CLOSE:
            handler.onLinkRemoteClose(this);
            break;
        case LINK_FLOW:
            handler.onLinkFlow(this);
            break;
        case LINK_FINAL:
            handler.onLinkFinal(this);
            break;
        case DELIVERY:
            handler.onDelivery(this);
            break;
        case TRANSPORT:
            handler.onTransport(this);
            break;
        case TRANSPORT_ERROR:
            handler.onTransportError(this);
            break;
        case TRANSPORT_HEAD_CLOSED:
            handler.onTransportHeadClosed(this);
            break;
        case TRANSPORT_TAIL_CLOSED:
            handler.onTransportTailClosed(this);
            break;
        case TRANSPORT_CLOSED:
            handler.onTransportClosed(this);
            break;
        case REACTOR_FINAL:
            handler.onReactorFinal(this);
            break;
        case REACTOR_QUIESCED:
            handler.onReactorQuiesced(this);
            break;
        case REACTOR_INIT:
            handler.onReactorInit(this);
            break;
        case SELECTABLE_ERROR:
            handler.onSelectableError(this);
            break;
        case SELECTABLE_EXPIRED:
            handler.onSelectableExpired(this);
            break;
        case SELECTABLE_FINAL:
            handler.onSelectableFinal(this);
            break;
        case SELECTABLE_INIT:
            handler.onSelectableInit(this);
            break;
        case SELECTABLE_READABLE:
            handler.onSelectableReadable(this);
            break;
        case SELECTABLE_UPDATED:
            handler.onSelectableWritable(this);
            break;
        case SELECTABLE_WRITABLE:
            handler.onSelectableWritable(this);
            break;
        case TIMER_TASK:
            handler.onTimerTask(this);
            break;
        default:
            handler.onUnhandled(this);
            break;
        }

        Iterator<Handler> children = handler.children();
        while(children.hasNext()) {
            dispatch(children.next());
        }
    }

    @Override
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

    @Override
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

    @Override
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

    @Override
    public Delivery getDelivery()
    {
        if (context instanceof Delivery) {
            return (Delivery) context;
        } else {
            return null;
        }
    }

    @Override
    public Transport getTransport()
    {
        if (context instanceof Transport) {
            return (Transport) context;
        } else {
            return null;
        }
    }

    @Override
    public Selectable getSelectable() {
        if (context instanceof Selectable) {
            return (Selectable) context;
        } else {
            return null;
        }
    }

    @Override
    public Reactor getReactor() {
        if (context instanceof Reactor) {
            return (Reactor) context;
        } else if (context instanceof Task) {
            return ((Task)context).getReactor();
        } else if (context instanceof Transport) {
            return ((TransportImpl)context).getReactor();
        } else if (context instanceof Delivery) {
            return ((Delivery)context).getLink().getSession().getConnection().getReactor();
        } else if (context instanceof Link) {
            return ((Link)context).getSession().getConnection().getReactor();
        } else if (context instanceof Session) {
            return ((Session)context).getConnection().getReactor();
        } else if (context instanceof Connection) {
            return ((Connection)context).getReactor();
        } else if (context instanceof Selectable) {
            return ((Selectable)context).getReactor();
        }
        return null;
    }

    @Override
    public Task getTask() {
        if (context instanceof Task) {
            return (Task) context;
        } else {
            return null;
        }
    }

    @Override
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
