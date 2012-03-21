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
import org.apache.qpid.proton.engine.EndpointState;
import org.apache.qpid.proton.engine.Sequence;
import org.apache.qpid.proton.engine.Session;

public class SessionImpl extends EndpointImpl implements Session
{
    private final ConnectionImpl _connection;
    
    private List<SenderImpl> _senders = new ArrayList<SenderImpl>();
    private List<ReceiverImpl> _receivers = new ArrayList<ReceiverImpl>();
    private TransportSession _transportSession;


    public SessionImpl(ConnectionImpl connection)
    {
        _connection = connection;
    }

    public void open()
    {
        super.open();
        modified();
    }

    public void close()
    {

        super.close();
        modified();
    }

    public SenderImpl sender(String name)
    {
        SenderImpl sender = new SenderImpl(this, name);
        _senders.add(sender);
        _connection.addEndpoint(sender);
        return sender;
    }
    
    public ReceiverImpl receiver(String name)
    {
        ReceiverImpl receiver = new ReceiverImpl(this, name);
        _receivers.add(receiver);
        _connection.addEndpoint(receiver);
        return receiver;
    }

    public Sequence<LinkImpl> endpoints(final EnumSet<EndpointState> local, final EnumSet<EndpointState> remote)
    {
        IteratorSequence<LinkImpl> senderSequence = new IteratorSequence<LinkImpl>(_senders.iterator());
        IteratorSequence<LinkImpl> receiverSequence = new IteratorSequence<LinkImpl>(_receivers.iterator());
        
        return new EndpointSelectionSequence<LinkImpl>(local, remote,
                                              new ConcatenatedSequence<LinkImpl>(senderSequence,receiverSequence));
    }

    @Override
    protected ConnectionImpl getConnectionImpl()
    {
        return _connection;
    }

    public void destroy()
    {
        super.destroy();
        _connection.removeEndpoint(this);
        for(SenderImpl sender : _senders)
        {
            sender.destroy();
        }
        _senders.clear();
        for(ReceiverImpl receiver : _receivers)
        {
            receiver.destroy();
        }
        _receivers.clear();
    }

    TransportSession getTransportSession()
    {
        return _transportSession;
    }

    void setTransportSession(TransportSession transportSession)
    {
        _transportSession = transportSession;
    }
}
