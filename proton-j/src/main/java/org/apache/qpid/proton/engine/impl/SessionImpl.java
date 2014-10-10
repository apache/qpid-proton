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

import java.util.*;
import org.apache.qpid.proton.engine.EndpointState;
import org.apache.qpid.proton.engine.ProtonJSession;
import org.apache.qpid.proton.engine.Session;
import org.apache.qpid.proton.engine.Event;

public class SessionImpl extends EndpointImpl implements ProtonJSession
{
    private final ConnectionImpl _connection;

    private Map<String, SenderImpl> _senders = new LinkedHashMap<String, SenderImpl>();
    private Map<String, ReceiverImpl>  _receivers = new LinkedHashMap<String, ReceiverImpl>();
    private TransportSession _transportSession;
    private int _incomingCapacity = 1024*1024;
    private int _incomingBytes = 0;
    private int _outgoingBytes = 0;
    private int _incomingDeliveries = 0;
    private int _outgoingDeliveries = 0;

    private LinkNode<SessionImpl> _node;


    SessionImpl(ConnectionImpl connection)
    {
        _connection = connection;
        _connection.incref();
        _node = _connection.addSessionEndpoint(this);
        _connection.put(Event.Type.SESSION_INIT, this);
    }

    public SenderImpl sender(String name)
    {
        SenderImpl sender = _senders.get(name);
        if(sender == null)
        {
            sender = new SenderImpl(this, name);
            _senders.put(name, sender);
        }
        return sender;
    }

    public ReceiverImpl receiver(String name)
    {
        ReceiverImpl receiver = _receivers.get(name);
        if(receiver == null)
        {
            receiver = new ReceiverImpl(this, name);
            _receivers.put(name, receiver);
        }
        return receiver;
    }

    public Session next(EnumSet<EndpointState> local, EnumSet<EndpointState> remote)
    {
        LinkNode.Query<SessionImpl> query = new EndpointImplQuery<SessionImpl>(local, remote);

        LinkNode<SessionImpl> sessionNode = _node.next(query);

        return sessionNode == null ? null : sessionNode.getValue();
    }

    @Override
    protected ConnectionImpl getConnectionImpl()
    {
        return _connection;
    }

    public ConnectionImpl getConnection()
    {
        return getConnectionImpl();
    }

    @Override
    void postFinal() {
        _connection.put(Event.Type.SESSION_FINAL, this);
        _connection.decref();
    }

    @Override
    void doFree() {
        _connection.removeSessionEndpoint(_node);
        _node = null;

        List<SenderImpl> senders = new ArrayList<SenderImpl>(_senders.values());
        for(SenderImpl sender : senders) {
            sender.free();
        }
        _senders.clear();

        List<ReceiverImpl> receivers = new ArrayList<ReceiverImpl>(_receivers.values());
        for(ReceiverImpl receiver : receivers) {
            receiver.free();
        }
        _receivers.clear();
    }

    void modifyEndpoints() {
        for (SenderImpl snd : _senders.values()) {
            snd.modifyEndpoints();
        }

        for (ReceiverImpl rcv : _receivers.values()) {
            rcv.modifyEndpoints();
        }
        modified();
    }

    TransportSession getTransportSession()
    {
        return _transportSession;
    }

    void setTransportSession(TransportSession transportSession)
    {
        _transportSession = transportSession;
    }

    void setNode(LinkNode<SessionImpl> node)
    {
        _node = node;
    }

    void freeSender(SenderImpl sender)
    {
        _senders.remove(sender.getName());
    }

    void freeReceiver(ReceiverImpl receiver)
    {
        _receivers.remove(receiver.getName());
    }

    @Override
    public int getIncomingCapacity()
    {
        return _incomingCapacity;
    }

    @Override
    public void setIncomingCapacity(int capacity)
    {
        _incomingCapacity = capacity;
    }

    @Override
    public int getIncomingBytes()
    {
        return _incomingBytes;
    }

    void incrementIncomingBytes(int delta)
    {
        _incomingBytes += delta;
    }

    @Override
    public int getOutgoingBytes()
    {
        return _outgoingBytes;
    }

    void incrementOutgoingBytes(int delta)
    {
        _outgoingBytes += delta;
    }

    void incrementIncomingDeliveries(int delta)
    {
        _incomingDeliveries += delta;
    }

    int getOutgoingDeliveries()
    {
        return _outgoingDeliveries;
    }

    void incrementOutgoingDeliveries(int delta)
    {
        _outgoingDeliveries += delta;
    }

    @Override
    void localOpen()
    {
        getConnectionImpl().put(Event.Type.SESSION_OPEN, this);
    }

    @Override
    void localClose()
    {
        getConnectionImpl().put(Event.Type.SESSION_CLOSE, this);
    }
}
