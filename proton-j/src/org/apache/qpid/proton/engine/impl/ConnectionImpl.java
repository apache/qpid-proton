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

import java.util.ArrayList;
import java.util.EnumSet;
import java.util.List;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.EndpointState;
import org.apache.qpid.proton.engine.Sequence;
import org.apache.qpid.proton.engine.Session;
import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.type.transport.Open;

public class ConnectionImpl extends EndpointImpl implements Connection
{

    public static final int MAX_CHANNELS = 255;
    private TransportFactory _transportFactory = TransportFactory.getDefaultTransportFactory();
    private TransportImpl _transport;
    private List<SessionImpl> _sessions = new ArrayList<SessionImpl>();
    private EndpointImpl _transportTail;
    private EndpointImpl _transportHead;
    private int _maxChannels = MAX_CHANNELS;
    private EndpointImpl _tail;
    private EndpointImpl _head;

    private DeliveryImpl _workHead;
    private DeliveryImpl _workTail;

    private DeliveryImpl _transportWorkHead;
    private DeliveryImpl _transportWorkTail;

    public ConnectionImpl() 
    {
        _transportFactory = TransportFactory.getDefaultTransportFactory();
        _head = this;
        _tail = this;
    }

    public SessionImpl session()
    {
        SessionImpl session = new SessionImpl(this);
        _sessions.add(session);
        addEndpoint(session);
        return session;
    }

    protected void addEndpoint(EndpointImpl endpoint)
    {
        endpoint.setPrev(_tail);
        _tail.setNext(endpoint);
        _tail = endpoint;
    }

    protected void removeEndpoint(EndpointImpl endpoint)
    {
        if(endpoint == _tail)
        {
            _tail = endpoint.getPrev();
        }
        if(endpoint == _head)
        {
            _head = endpoint.getNext();
        }

    }

    public Transport transport()
    {
        if(_transport == null)
        {
            _transport = (TransportImpl) _transportFactory.transport(this);
            addEndpoint(_transport);
        }
        else
        {
            // todo - should error

        }
        return _transport;
    }

    public Sequence<? extends EndpointImpl> endpoints(final EnumSet<EndpointState> local, 
                                                      final EnumSet<EndpointState> remote)
    {
        return new EndpointSelectionSequence<EndpointImpl>(local, remote, new EndpointSequence(this));
    }

    @Override
    protected ConnectionImpl getConnectionImpl()
    {
        return this;
    }

    public void destroy()
    {
        super.destroy();
        for(Session session : _sessions)
        {
            session.destroy();
        }
        _sessions = null;
        if(_transport != null)
        {
            _transport.destroy();
        }
    }

    void clearTransport() 
    {
        removeEndpoint(_transport);
        _transport = null;
    }

    public void handleOpen(Open open)
    {
        // TODO - store state
        setRemoteState(EndpointState.ACTIVE);
    }


    EndpointImpl getTransportHead()
    {
        return _transportHead;
    }

    EndpointImpl getTransportTail()
    {
        return _transportTail;
    }

    void addModified(EndpointImpl endpoint)
    {
        if(_transportTail == null)
        {
            _transportHead = _transportTail = endpoint;
        }
        else
        {
            _transportTail.setTransportNext(endpoint);
            endpoint.setTransportPrev(_transportTail);
            _transportTail = endpoint;
        }
    }

    void removeModified(EndpointImpl endpoint)
    {
        if(_transportHead == endpoint)
        {
            _transportHead = endpoint.transportNext();
        }
        else
        {
            endpoint.transportPrev().setTransportNext(endpoint.transportNext());
        }

        if(_transportTail == endpoint)
        {
            _transportTail = endpoint.transportPrev();
        }
        else
        {
            endpoint.transportNext().setTransportPrev(endpoint.transportPrev());
        }
    }

    public int getMaxChannels()
    {
        return _maxChannels;
    }

    public EndpointImpl next()
    {
        return getNext();
    }


    private static class EndpointSequence implements Sequence<EndpointImpl>
    {
        private EndpointImpl _current;

        public EndpointSequence(ConnectionImpl connection)
        {
            _current = connection;
        }


        public EndpointImpl next()
        {
            EndpointImpl next = _current;
            if(next != null)
            {
                _current = next.getNext();
            }
            return next;
        }
    }

    DeliveryImpl getWorkHead()
    {
        return _workHead;
    }

    DeliveryImpl getWorkTail()
    {
        return _workTail;
    }
    
    void removeWork(DeliveryImpl delivery)
    {
        if(_workHead == delivery)
        {
            _workHead = delivery.getWorkNext();

        }
        if(_workTail == delivery)
        {
            _workTail = delivery.getWorkPrev();    
        }
    }
    
    void addWork(DeliveryImpl delivery)
    {
        if(_workHead != delivery && delivery.getWorkNext() == null && delivery.getWorkPrev() == null)
        {
            if(_workTail == null)
            {
                _workHead = _workTail = delivery;
            }
            else
            {
                _workTail.setWorkNext(delivery);
                delivery.setWorkPrev(_workTail);
                _workTail = delivery;
            }
        }
    }

    public Sequence<DeliveryImpl> getWorkSequence()
    {
        return new WorkSequence(_workHead);
    }

    private class WorkSequence implements Sequence<DeliveryImpl>
    {
        private DeliveryImpl _next;

        public WorkSequence(DeliveryImpl workHead)
        {
            _next = workHead;
        }

        public DeliveryImpl next()
        {
            DeliveryImpl next = _next;
            if(next != null)
            {
                _next = next.getWorkNext();
            }
            return next;
        }
    }

    DeliveryImpl getTransportWorkHead()
    {
        return _transportWorkHead;
    }

    public void removeTransportWork(DeliveryImpl delivery)
    {
        DeliveryImpl oldHead = _transportWorkHead;
        DeliveryImpl oldTail = _transportWorkTail;
        if(_transportWorkHead == delivery)
        {
            _transportWorkHead = delivery.getTransportWorkNext();

        }
        if(_transportWorkTail == delivery)
        {
            _transportWorkTail = delivery.getTransportWorkPrev();
        }
    }


    void addTransportWork(DeliveryImpl delivery)
    {
        if(_transportWorkTail == null)
        {
            _transportWorkHead = _transportWorkTail = delivery;
        }
        else
        {
            _transportWorkTail.setTransportWorkNext(delivery);
            delivery.setTransportWorkPrev(_transportWorkTail);
            _transportWorkTail = delivery;
        }
    }

    void workUpdate(DeliveryImpl delivery)
    {
        if(delivery != null)
        {
            LinkImpl link = delivery.getLink();
            if(link.workUpdate(delivery))
            {
                addWork(delivery);
            }
            else
            {
                delivery.clearWork();
            }
        }
    }
}
