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
import org.apache.qpid.proton.engine.*;
import org.apache.qpid.proton.type.transport.Open;

public class ConnectionImpl extends EndpointImpl implements Connection
{

    public static final int MAX_CHANNELS = 255;
    private List<SessionImpl> _sessions = new ArrayList<SessionImpl>();
    private EndpointImpl _transportTail;
    private EndpointImpl _transportHead;
    private int _maxChannels = MAX_CHANNELS;

    private LinkNode<SessionImpl> _sessionHead;
    private LinkNode<SessionImpl> _sessionTail;


    private LinkNode<LinkImpl> _linkHead;
    private LinkNode<LinkImpl> _linkTail;


    private DeliveryImpl _workHead;
    private DeliveryImpl _workTail;

    private DeliveryImpl _transportWorkHead;
    private DeliveryImpl _transportWorkTail;
    private String _localContainerId = "";
    private String _localHostname = "";
    private boolean _bound;
    private String _remoteContainer;
    private String _remoteHostname;

    public ConnectionImpl()
    {
    }

    public SessionImpl session()
    {
        SessionImpl session = new SessionImpl(this);
        _sessions.add(session);


        return session;
    }

    protected LinkNode<SessionImpl> addSessionEndpoint(SessionImpl endpoint)
    {
        LinkNode<SessionImpl> node;
        if(_sessionHead == null)
        {
            node = _sessionHead = _sessionTail = LinkNode.newList(endpoint);
        }
        else
        {
            node = _sessionTail = _sessionTail.addAtTail(endpoint);
        }
        return node;
    }

    void removeSessionEndpoint(LinkNode<SessionImpl> node)
    {
        LinkNode<SessionImpl> prev = node.getPrev();
        LinkNode<SessionImpl> next = node.getNext();

        if(_sessionHead == node)
        {
            _sessionHead = next;
        }
        if(_sessionTail == node)
        {
            _sessionTail = prev;
        }
        node.remove();
    }


    protected LinkNode<LinkImpl> addLinkEndpoint(LinkImpl endpoint)
    {
        LinkNode<LinkImpl> node;
        if(_linkHead == null)
        {
            node = _linkHead = _linkTail = LinkNode.newList(endpoint);
        }
        else
        {
            node = _linkTail = _linkTail.addAtTail(endpoint);
        }
        return node;
    }


    void removeLinkEndpoint(LinkNode<LinkImpl> node)
    {
        LinkNode<LinkImpl> prev = node.getPrev();
        LinkNode<LinkImpl> next = node.getNext();

        if(_linkHead == node)
        {
            _linkHead = next;
        }
        if(_linkTail == node)
        {
            _linkTail = prev;
        }
        node.remove();
    }


    public Session sessionHead(final EnumSet<EndpointState> local, final EnumSet<EndpointState> remote)
    {
        if(_sessionHead == null)
        {
            return null;
        }
        else
        {
            LinkNode.Query<SessionImpl> query = new EndpointImplQuery<SessionImpl>(local, remote);
            LinkNode<SessionImpl> node = query.matches(_sessionHead) ? _sessionHead : _sessionHead.next(query);
            return node == null ? null : node.getValue();
        }
    }

    public Link linkHead(EnumSet<EndpointState> local, EnumSet<EndpointState> remote)
    {
        if(_linkHead == null)
        {
            return null;
        }
        else
        {
            LinkNode.Query<LinkImpl> query = new EndpointImplQuery<LinkImpl>(local, remote);
            LinkNode<LinkImpl> node = query.matches(_linkHead) ? _linkHead : _linkHead.next(query);
            return node == null ? null : node.getValue();
        }
    }

    @Override
    protected ConnectionImpl getConnectionImpl()
    {
        return this;
    }

    public void free()
    {
        super.free();
        for(Session session : _sessions)
        {
            session.free();
        }
        _sessions = null;
    }

    public void handleOpen(Open open)
    {
        // TODO - store state
        setRemoteState(EndpointState.ACTIVE);
        setRemoteHostname(open.getHostname());
        setRemoteContainer(open.getContainerId());
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
            endpoint.setTransportNext(null);
            endpoint.setTransportPrev(null);
            _transportHead = _transportTail = endpoint;
        }
        else
        {
            _transportTail.setTransportNext(endpoint);
            endpoint.setTransportPrev(_transportTail);
            _transportTail = endpoint;
            _transportTail.setTransportNext(null);
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

    public String getLocalContainerId()
    {
        return _localContainerId;
    }

    public void setLocalContainerId(String localContainerId)
    {
        _localContainerId = localContainerId;
    }

    public DeliveryImpl getWorkHead()
    {
        return _workHead;
    }

    @Override
    public void setContainer(String container)
    {
        _localContainerId = container;
    }

    @Override
    public void setHostname(String hostname)
    {
        _localHostname = hostname;
    }

    @Override
    public String getRemoteContainer()
    {
        return _remoteContainer;
    }

    @Override
    public String getRemoteHostname()
    {
        return _remoteHostname;
    }

    public String getHostname()
    {
        return _localHostname;
    }

    void setRemoteContainer(String remoteContainerId)
    {
        _remoteContainer = remoteContainerId;
    }

    void setRemoteHostname(String remoteHostname)
    {
        _remoteHostname = remoteHostname;
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
                delivery.setWorkNext(null);
                delivery.setWorkPrev(null);
                _workHead = _workTail = delivery;
            }
            else
            {
                _workTail.setWorkNext(delivery);
                delivery.setWorkPrev(_workTail);
                _workTail = delivery;
                delivery.setWorkNext(null);
            }
        }
    }

    public Sequence<DeliveryImpl> getWorkSequence()
    {
        return new WorkSequence(_workHead);
    }

    public void setBound(boolean bound)
    {
        _bound = true;
    }

    public boolean isBound()
    {
        return _bound;
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
            delivery.setTransportWorkNext(null);
            delivery.setTransportWorkPrev(null);
            _transportWorkHead = _transportWorkTail = delivery;
        }
        else
        {
            _transportWorkTail.setTransportWorkNext(delivery);
            delivery.setTransportWorkPrev(_transportWorkTail);
            _transportWorkTail = delivery;
            delivery.setTransportWorkNext(null);
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
