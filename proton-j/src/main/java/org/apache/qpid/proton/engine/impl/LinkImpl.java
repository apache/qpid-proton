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

import java.util.EnumSet;
import org.apache.qpid.proton.amqp.transport.ReceiverSettleMode;
import org.apache.qpid.proton.amqp.transport.SenderSettleMode;
import org.apache.qpid.proton.engine.EndpointState;
import org.apache.qpid.proton.engine.Link;
import org.apache.qpid.proton.amqp.transport.Source;
import org.apache.qpid.proton.amqp.transport.Target;
import org.apache.qpid.proton.engine.Event;

public abstract class LinkImpl extends EndpointImpl implements Link
{

    private final SessionImpl _session;

    DeliveryImpl _head;
    DeliveryImpl _tail;
    DeliveryImpl _current;
    private String _name;
    private Source _source;
    private Source _remoteSource;
    private Target _target;
    private Target _remoteTarget;
    private int _queued;
    private int _credit;
    private int _unsettled;
    private int _drained;

    private SenderSettleMode _senderSettleMode;
    private SenderSettleMode _remoteSenderSettleMode;
    private ReceiverSettleMode _receiverSettleMode;
    private ReceiverSettleMode _remoteReceiverSettleMode;


    private LinkNode<LinkImpl> _node;
    private boolean _drain;

    LinkImpl(SessionImpl session, String name)
    {
        _session = session;
        _session.incref();
        _name = name;
        ConnectionImpl conn = session.getConnectionImpl();
        _node = conn.addLinkEndpoint(this);
        conn.put(Event.Type.LINK_INIT, this);
    }


    public String getName()
    {
        return _name;
    }

    public DeliveryImpl delivery(byte[] tag)
    {
        return delivery(tag, 0, tag.length);
    }

    public DeliveryImpl delivery(byte[] tag, int offset, int length)
    {
        if (offset != 0 || length != tag.length)
        {
            throw new IllegalArgumentException("At present delivery tag must be the whole byte array");
        }
        incrementQueued();
        try
        {
            DeliveryImpl delivery = new DeliveryImpl(tag, this, _tail);
            if (_tail == null)
            {
                _head = delivery;
            }
            _tail = delivery;
            if (_current == null)
            {
                _current = delivery;
            }
            getConnectionImpl().workUpdate(delivery);
            return delivery;
        }
        catch (RuntimeException e)
        {
            e.printStackTrace();
            throw e;
        }
    }

    @Override
    void postFinal() {
        _session.getConnectionImpl().put(Event.Type.LINK_FINAL, this);
        _session.decref();
    }

    @Override
    void doFree()
    {
        _session.getConnectionImpl().removeLinkEndpoint(_node);
        _node = null;
    }

    void modifyEndpoints() {
        modified();
    }

    public void remove(DeliveryImpl delivery)
    {
        if(_head == delivery)
        {
            _head = delivery.getLinkNext();
        }
        if(_tail == delivery)
        {
            _tail = delivery.getLinkPrevious();
        }
        if(_current == delivery)
        {
            // TODO - what???
        }
    }

    public DeliveryImpl current()
    {
        return _current;
    }

    public boolean advance()
    {
        if(_current != null )
        {
            DeliveryImpl oldCurrent = _current;
            _current = _current.getLinkNext();
            getConnectionImpl().workUpdate(oldCurrent);

            if(_current != null)
            {
                getConnectionImpl().workUpdate(_current);
            }
            return true;
        }
        else
        {
            return false;
        }

    }

    @Override
    protected ConnectionImpl getConnectionImpl()
    {
        return _session.getConnectionImpl();
    }

    public SessionImpl getSession()
    {
        return _session;
    }

    public Source getRemoteSource()
    {
        return _remoteSource;
    }

    void setRemoteSource(Source source)
    {
        _remoteSource = source;
    }

    public Target getRemoteTarget()
    {
        return _remoteTarget;
    }

    void setRemoteTarget(Target target)
    {
        _remoteTarget = target;
    }

    public Source getSource()
    {
        return _source;
    }

    public void setSource(Source source)
    {
        // TODO - should be an error if local state is ACTIVE
        _source = source;
    }

    public Target getTarget()
    {
        return _target;
    }

    public void setTarget(Target target)
    {
        // TODO - should be an error if local state is ACTIVE
        _target = target;
    }

    public Link next(EnumSet<EndpointState> local, EnumSet<EndpointState> remote)
    {
        LinkNode.Query<LinkImpl> query = new EndpointImplQuery<LinkImpl>(local, remote);

        LinkNode<LinkImpl> linkNode = _node.next(query);

        return linkNode == null ? null : linkNode.getValue();

    }

    abstract TransportLink getTransportLink();

    public int getCredit()
    {
        return _credit;
    }

    public void addCredit(int credit)
    {
        _credit+=credit;
    }

    public void setCredit(int credit)
    {
        _credit = credit;
    }

    boolean hasCredit()
    {
        return _credit > 0;
    }

    void incrementCredit()
    {
        _credit++;
    }

    void decrementCredit()
    {
        _credit--;
    }

    public int getQueued()
    {
        return _queued;
    }

    void incrementQueued()
    {
        _queued++;
    }

    void decrementQueued()
    {
        _queued--;
    }

    public int getUnsettled()
    {
        return _unsettled;
    }

    void incrementUnsettled()
    {
        _unsettled++;
    }

    void decrementUnsettled()
    {
        _unsettled--;
    }

    void setDrain(boolean drain)
    {
        _drain = drain;
    }

    public boolean getDrain()
    {
        return _drain;
    }

    @Override
    public SenderSettleMode getSenderSettleMode()
    {
        return _senderSettleMode;
    }

    @Override
    public void setSenderSettleMode(SenderSettleMode senderSettleMode)
    {
        _senderSettleMode = senderSettleMode;
    }

    @Override
    public SenderSettleMode getRemoteSenderSettleMode()
    {
        return _remoteSenderSettleMode;
    }

    @Override
    public void setRemoteSenderSettleMode(SenderSettleMode remoteSenderSettleMode)
    {
        _remoteSenderSettleMode = remoteSenderSettleMode;
    }

    @Override
    public ReceiverSettleMode getReceiverSettleMode()
    {
        return _receiverSettleMode;
    }

    @Override
    public void setReceiverSettleMode(ReceiverSettleMode receiverSettleMode)
    {
        _receiverSettleMode = receiverSettleMode;
    }

    @Override
    public ReceiverSettleMode getRemoteReceiverSettleMode()
    {
        return _remoteReceiverSettleMode;
    }

    void setRemoteReceiverSettleMode(ReceiverSettleMode remoteReceiverSettleMode)
    {
        _remoteReceiverSettleMode = remoteReceiverSettleMode;
    }

    public int drained()
    {
        int drained = 0;

        if (this instanceof SenderImpl) {
            if(getDrain() && hasCredit())
            {
                _drained = getCredit();
                setCredit(0);
                modified();
                drained = _drained;
            }
        } else {
            drained = _drained;
            _drained = 0;
        }

        return drained;
    }

    int getDrained()
    {
        return _drained;
    }

    void setDrained(int value)
    {
        _drained = value;
    }

    public int getRemoteCredit()
    {
        return _credit - _queued;
    }

    public DeliveryImpl head()
    {
        return _head;
    }

    @Override
    void localOpen()
    {
        getConnectionImpl().put(Event.Type.LINK_OPEN, this);
    }

    @Override
    void localClose()
    {
        getConnectionImpl().put(Event.Type.LINK_CLOSE, this);
    }
}
