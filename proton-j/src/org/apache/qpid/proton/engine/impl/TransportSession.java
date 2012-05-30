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

import java.util.HashMap;
import java.util.Map;
import org.apache.qpid.proton.type.Binary;
import org.apache.qpid.proton.type.UnsignedInteger;
import org.apache.qpid.proton.type.transport.Disposition;
import org.apache.qpid.proton.type.transport.Flow;
import org.apache.qpid.proton.type.transport.Transfer;

class TransportSession
{
    private final SessionImpl _session;
    private int _localChannel = -1;
    private int _remoteChannel = -1;
    private boolean _openSent;
    private UnsignedInteger       _handleMax = UnsignedInteger.valueOf(1024);
    private UnsignedInteger _incomingWindowSize = UnsignedInteger.valueOf(1024);
    private UnsignedInteger _outgoingWindowSize = UnsignedInteger.valueOf(1024);
    private UnsignedInteger _nextOutgoingId = UnsignedInteger.ONE;
    private TransportLink[] _remoteHandleMap = new TransportLink[1024];
    private TransportLink[] _localHandleMap = new TransportLink[1024];
    private Map<String, TransportLink> _halfOpenLinks = new HashMap<String, TransportLink>();


    private UnsignedInteger _currentDeliveryId;
    private UnsignedInteger _remoteIncomingWindow;
    private UnsignedInteger _remoteOutgoingWindow;
    private UnsignedInteger _remoteNextIncomingId;
    private UnsignedInteger _remoteNextOutgoingId;

    public TransportSession(SessionImpl session)
    {
        _session = session;
    }

    public SessionImpl getSession()
    {
        return _session;
    }

    public int getLocalChannel()
    {
        return _localChannel;
    }

    public void setLocalChannel(int localChannel)
    {
        _localChannel = localChannel;
    }

    public int getRemoteChannel()
    {
        return _remoteChannel;
    }

    public void setRemoteChannel(int remoteChannel)
    {
        _remoteChannel = remoteChannel;
    }

    public boolean isOpenSent()
    {
        return _openSent;
    }

    public void setOpenSent(boolean openSent)
    {
        _openSent = openSent;
    }

    public boolean isRemoteChannelSet()
    {
        return _remoteChannel != -1;
    }

    public boolean isLocalChannelSet()
    {
        return _localChannel != -1;
    }

    public void unsetLocalChannel()
    {
        _localChannel = -1;
    }

    public void unsetRemoteChannel()
    {
        _remoteChannel = -1;
    }


    public UnsignedInteger getHandleMax()
    {
        return _handleMax;
    }

    public UnsignedInteger getIncomingWindowSize()
    {
        return _incomingWindowSize;
    }

    public UnsignedInteger getOutgoingWindowSize()
    {
        return _outgoingWindowSize;
    }

    public UnsignedInteger getNextOutgoingId()
    {
        return _nextOutgoingId;
    }

    public TransportLink getLinkFromRemoteHandle(UnsignedInteger handle)
    {
        return _remoteHandleMap[handle.intValue()];
    }

    public UnsignedInteger allocateLocalHandle()
    {
        for(int i = 0; i < _localHandleMap.length; i++)
        {
            if(_localHandleMap[i] == null)
            {
                return UnsignedInteger.valueOf(i);
            }
        }
        // TODO - error
        return UnsignedInteger.MAX_VALUE;
    }

    public void addLinkRemoteHandle(TransportLink link, UnsignedInteger remoteHandle)
    {
        _remoteHandleMap[remoteHandle.intValue()] = link;
    }

    public void addLinkLocalHandle(TransportLink link, UnsignedInteger localhandle)
    {
        _localHandleMap[localhandle.intValue()] = link;
    }

    public void freeLocalHandle(UnsignedInteger handle)
    {
        _localHandleMap[handle.intValue()] = null;
    }

    public void freeRemoteHandle(UnsignedInteger handle)
    {
        _remoteHandleMap[handle.intValue()] = null;
    }

    public TransportLink resolveHalfOpenLink(String name)
    {
        return _halfOpenLinks.remove(name);
    }

    public void addHalfOpenLink(TransportLink link)
    {
        _halfOpenLinks.put(link.getName(), link);
    }

    public void handleTransfer(Transfer transfer, Binary payload)
    {

        if(transfer.getDeliveryId() == null || transfer.getDeliveryId().equals(_currentDeliveryId))
        {

            // TODO - handle large messages
        }
        else
        {
            // TODO - check deliveryId has been incremented by one
            _currentDeliveryId = transfer.getDeliveryId();
            // TODO - check link handle valid and a receiver
            TransportReceiver transportReceiver = (TransportReceiver) getLinkFromRemoteHandle(transfer.getHandle());
            ReceiverImpl receiver = transportReceiver.getReceiver();
            Binary deliveryTag = transfer.getDeliveryTag();
            DeliveryImpl delivery = receiver.delivery(deliveryTag.getArray(), deliveryTag.getArrayOffset(),
                                                      deliveryTag.getLength());
            TransportDelivery transportDelivery = new TransportDelivery(_currentDeliveryId, delivery, transportReceiver);
            delivery.setTransportDelivery(transportDelivery);
            // TODO - should this be a copy?
            if(payload != null)
            {
                delivery.setData(payload.getArray());
                delivery.setDataLength(payload.getLength());
                delivery.setDataOffset(payload.getArrayOffset());
            }
            delivery.addIOWork();


        }

        if(!(transfer.getMore() || transfer.getAborted()))
        {
            _incomingWindowSize = _incomingWindowSize.subtract(UnsignedInteger.ONE);
        }

    }

    public void freeLocalChannel()
    {
        _localChannel = -1;
    }

    private void setRemoteIncomingWindow(UnsignedInteger incomingWindow)
    {
        _remoteIncomingWindow = incomingWindow;
    }

    private void setRemoteOutgoingWindow(UnsignedInteger outgoingWindow)
    {
        _remoteOutgoingWindow = outgoingWindow;
    }

    void handleFlow(Flow flow)
    {
        setRemoteIncomingWindow(flow.getIncomingWindow());
        setRemoteOutgoingWindow(flow.getOutgoingWindow());
        setRemoteNextIncomingId(flow.getNextIncomingId());
        setRemoteNextOutgoingId(flow.getNextOutgoingId());

        if(flow.getHandle() != null)
        {
            TransportLink transportLink = getLinkFromRemoteHandle(flow.getHandle());
            transportLink.handleFlow(flow);


        }

    }

    private void setRemoteNextOutgoingId(UnsignedInteger nextOutgoingId)
    {
        _remoteNextOutgoingId = nextOutgoingId;
    }

    private void setRemoteNextIncomingId(UnsignedInteger remoteNextIncomingId)
    {
        _remoteNextIncomingId = remoteNextIncomingId;
    }

    void handleDisposition(Disposition disposition)
    {
        //TODO - Implement.
    }
}
