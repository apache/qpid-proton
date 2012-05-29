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
import org.apache.qpid.proton.engine.EndpointState;
import org.apache.qpid.proton.engine.Link;

public abstract class LinkImpl extends EndpointImpl implements Link
{

    private final SessionImpl _session;

    DeliveryImpl _head;
    DeliveryImpl _tail;
    DeliveryImpl _current;
    private String _name;
    private String _localSourceAddress;
    private String _remoteSourceAddress;
    private String _localTargetAddress;
    private String _remoteTargetAddress;

    private LinkNode<LinkImpl> _node;


    public LinkImpl(SessionImpl session, String name)
    {
        _session = session;
        _name = name;
        _node = session.getConnectionImpl().addLinkEndpoint(this);
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

    String getName()
    {
        return _name;
    }



    public DeliveryImpl delivery(byte[] tag, int offset, int length)
    {
        DeliveryImpl delivery = new DeliveryImpl(tag, this, _tail);
        if(_tail == null)
        {
            _head = delivery;
        }
        _tail = delivery;
        if(_current == null)
        {
            _current = delivery;
        }
        getConnectionImpl().workUpdate(delivery);
        return delivery;
    }

    public void destroy()
    {
        super.destroy();
        _session.getConnectionImpl().removeLinkEndpoint(_node);
        //TODO.
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

    SessionImpl getSession()
    {
        return _session;
    }

    public String getRemoteSourceAddress()
    {
        return _remoteSourceAddress;
    }

    void setRemoteSourceAddress(String sourceAddress)
    {
        _remoteSourceAddress = sourceAddress;
    }

    public String getRemoteTargetAddress()
    {
        return _remoteTargetAddress;
    }

    void setRemoteTargetAddress(String targetAddress)
    {
        _remoteTargetAddress = targetAddress;
    }

    String getLocalSourceAddress()
    {
        return _localSourceAddress;
    }

    public void setLocalSourceAddress(String localSourceAddress)
    {
        // TODO - should be an error if local state is ACTIVE
        _localSourceAddress = localSourceAddress;
        modified();
    }

    String getLocalTargetAddress()
    {
        return _localTargetAddress;
    }

    public void setLocalTargetAddress(String localTargetAddress)
    {
        // TODO - should be an error if local state is ACTIVE
        _localTargetAddress = localTargetAddress;
        modified();
    }

    public Link next(EnumSet<EndpointState> local, EnumSet<EndpointState> remote)
    {
        LinkNode.Query<LinkImpl> query = new EndpointImplQuery<LinkImpl>(local, remote);

        LinkNode<LinkImpl> linkNode = _node.next(query);

        return linkNode == null ? null : linkNode.getValue();

    }

    abstract TransportLink getTransportLink();

    abstract boolean workUpdate(DeliveryImpl delivery);
}
