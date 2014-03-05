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
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.engine.*;
import org.apache.qpid.proton.amqp.transport.Open;

public class ConnectionImpl extends EndpointImpl implements ProtonJConnection
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

    private TransportImpl _transport;
    private DeliveryImpl _transportWorkHead;
    private DeliveryImpl _transportWorkTail;
    private String _localContainerId = "";
    private String _localHostname = "";
    private String _remoteContainer;
    private String _remoteHostname;
    private Symbol[] _offeredCapabilities;
    private Symbol[] _desiredCapabilities;
    private Symbol[] _remoteOfferedCapabilities;
    private Symbol[] _remoteDesiredCapabilities;
    private Map<Symbol, Object> _properties;
    private Map<Symbol, Object> _remoteProperties;

    private Object _context;
    private CollectorImpl _collector;

    private static final Symbol[] EMPTY_SYMBOL_ARRAY = new Symbol[0];

    /**
     * @deprecated This constructor's visibility will be reduced to the default scope in a future release.
     * Client code outside this module should use a {@link EngineFactory} instead
     */
    @Deprecated public ConnectionImpl()
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

    void handleOpen(Open open)
    {
        // TODO - store state
        setRemoteState(EndpointState.ACTIVE);
        setRemoteHostname(open.getHostname());
        setRemoteContainer(open.getContainerId());
        setRemoteDesiredCapabilities(open.getDesiredCapabilities());
        setRemoteOfferedCapabilities(open.getOfferedCapabilities());
        setRemoteProperties(open.getProperties());
        EventImpl ev = put(Event.Type.CONNECTION_STATE);
        if (ev != null) {
            ev.init(this);
        }
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

    @Override
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

    @Override
    public void setOfferedCapabilities(Symbol[] capabilities)
    {
        _offeredCapabilities = capabilities;
    }

    @Override
    public void setDesiredCapabilities(Symbol[] capabilities)
    {
        _desiredCapabilities = capabilities;
    }

    @Override
    public Symbol[] getRemoteOfferedCapabilities()
    {
        return _remoteOfferedCapabilities == null ? EMPTY_SYMBOL_ARRAY : _remoteOfferedCapabilities;
    }

    @Override
    public Symbol[] getRemoteDesiredCapabilities()
    {
        return _remoteDesiredCapabilities == null ? EMPTY_SYMBOL_ARRAY : _remoteDesiredCapabilities;
    }


    Symbol[] getOfferedCapabilities()
    {
        return _offeredCapabilities;
    }

    Symbol[] getDesiredCapabilities()
    {
        return _desiredCapabilities;
    }

    void setRemoteOfferedCapabilities(Symbol[] remoteOfferedCapabilities)
    {
        _remoteOfferedCapabilities = remoteOfferedCapabilities;
    }

    void setRemoteDesiredCapabilities(Symbol[] remoteDesiredCapabilities)
    {
        _remoteDesiredCapabilities = remoteDesiredCapabilities;
    }


    Map<Symbol, Object> getProperties()
    {
        return _properties;
    }

    public void setProperties(Map<Symbol, Object> properties)
    {
        _properties = properties;
    }

    public Map<Symbol, Object> getRemoteProperties()
    {
        return _remoteProperties;
    }

    void setRemoteProperties(Map<Symbol, Object> remoteProperties)
    {
        _remoteProperties = remoteProperties;
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
        if (!delivery._work) return;

        DeliveryImpl next = delivery.getWorkNext();
        DeliveryImpl prev = delivery.getWorkPrev();

        if (prev != null) {
            prev.setWorkNext(next);
        }

        if (next != null) {
            next.setWorkPrev(prev);
        }


        if(_workHead == delivery)
        {
            _workHead = next;

        }

        if(_workTail == delivery)
        {
            _workTail = prev;
        }

        delivery._work = false;
    }

    void addWork(DeliveryImpl delivery)
    {
        if (delivery._work) return;

        delivery.setWorkNext(null);
        delivery.setWorkPrev(_workTail);

        if (_workTail != null) {
            _workTail.setWorkNext(delivery);
        }

        _workTail = delivery;

        if (_workHead == null) {
            _workHead = delivery;
        }

        delivery._work = true;
    }

    public Iterator<DeliveryImpl> getWorkSequence()
    {
        return new WorkSequence(_workHead);
    }

    void setTransport(TransportImpl transport)
    {
        _transport = transport;
    }

    TransportImpl getTransport()
    {
        return _transport;
    }

    private class WorkSequence implements Iterator<DeliveryImpl>
    {
        private DeliveryImpl _next;

        public WorkSequence(DeliveryImpl workHead)
        {
            _next = workHead;
        }

        @Override
        public boolean hasNext()
        {
            return _next != null;
        }

        @Override
        public void remove()
        {
            throw new UnsupportedOperationException();
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
        if (!delivery._transportWork) return;

        DeliveryImpl next = delivery.getTransportWorkNext();
        DeliveryImpl prev = delivery.getTransportWorkPrev();

        if (prev != null) {
            prev.setTransportWorkNext(next);
        }

        if (next != null) {
            next.setTransportWorkPrev(prev);
        }


        if(_transportWorkHead == delivery)
        {
            _transportWorkHead = next;

        }

        if(_transportWorkTail == delivery)
        {
            _transportWorkTail = prev;
        }

        delivery._transportWork = false;
    }

    void addTransportWork(DeliveryImpl delivery)
    {
        modified();
        if (delivery._transportWork) return;

        delivery.setTransportWorkNext(null);
        delivery.setTransportWorkPrev(_transportWorkTail);

        if (_transportWorkTail != null) {
            _transportWorkTail.setTransportWorkNext(delivery);
        }

        _transportWorkTail = delivery;

        if (_transportWorkHead == null) {
            _transportWorkHead = delivery;
        }

        delivery._transportWork = true;
    }

    void workUpdate(DeliveryImpl delivery)
    {
        if(delivery != null)
        {
            if(!delivery.isSettled() &&
               (delivery.isReadable() ||
                delivery.isWritable() ||
                delivery.isUpdated()))
            {
                addWork(delivery);
            }
            else
            {
                removeWork(delivery);
            }
        }
    }

    public Object getContext()
    {
        return _context;
    }

    public void setContext(Object context)
    {
        _context = context;
    }

    public void collect(Collector collector)
    {
        _collector = (CollectorImpl) collector;
    }

    EventImpl put(Event.Type type)
    {
        if (_collector != null) {
            return _collector.put(type);
        } else {
            return null;
        }
    }

}
