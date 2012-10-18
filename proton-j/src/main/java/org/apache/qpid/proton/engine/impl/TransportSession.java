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
    private UnsignedInteger _incomingWindowSize = UnsignedInteger.valueOf(TransportImpl.SESSION_WINDOW);
    private UnsignedInteger _outgoingWindowSize = UnsignedInteger.valueOf(TransportImpl.SESSION_WINDOW);
    private UnsignedInteger _nextOutgoingId = UnsignedInteger.ONE;
    private UnsignedInteger _nextIncomingId = null;

    private TransportLink[] _remoteHandleMap = new TransportLink[1024];
    private TransportLink[] _localHandleMap = new TransportLink[1024];
    private Map<String, TransportLink> _halfOpenLinks = new HashMap<String, TransportLink>();


    private UnsignedInteger _currentDeliveryId;
    private UnsignedInteger _remoteIncomingWindow;
    private UnsignedInteger _remoteOutgoingWindow;
    private UnsignedInteger _remoteNextIncomingId = _nextOutgoingId;
    private UnsignedInteger _remoteNextOutgoingId;
    private Map<UnsignedInteger, DeliveryImpl>
            _unsettledIncomingDeliveriesById = new HashMap<UnsignedInteger, DeliveryImpl>();
    private Map<UnsignedInteger, DeliveryImpl>
            _unsettledOutgoingDeliveriesById = new HashMap<UnsignedInteger, DeliveryImpl>();
    private int _unsettledIncomingSize;
    private boolean _incomingWindowSizeChange;
    private boolean _outgoingWindowSizeChange;
    private boolean _endReceived;
    private boolean _beginSent;

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

    public void incrementOutgoingWindow()
    {
        _outgoingWindowSize = _outgoingWindowSize.add(UnsignedInteger.ONE);
    }

    public void decrementOutgoingWindow()
    {
        _outgoingWindowSize = _outgoingWindowSize.subtract(UnsignedInteger.ONE);
    }



    public UnsignedInteger getNextOutgoingId()
    {
        return _nextOutgoingId;
    }

    public TransportLink getLinkFromRemoteHandle(UnsignedInteger handle)
    {
        return _remoteHandleMap[handle.intValue()];
    }

    public UnsignedInteger allocateLocalHandle(TransportLink transportLink)
    {
        for(int i = 0; i < _localHandleMap.length; i++)
        {
            if(_localHandleMap[i] == null)
            {
                UnsignedInteger rc = UnsignedInteger.valueOf(i);
                _localHandleMap[i] = transportLink;
                transportLink.setLocalHandle(rc);
                return rc;
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
        DeliveryImpl delivery;
        incrementNextIncomingId();
        if(transfer.getDeliveryId() == null || transfer.getDeliveryId().equals(_currentDeliveryId))
        {
            TransportReceiver transportReceiver = (TransportReceiver) getLinkFromRemoteHandle(transfer.getHandle());
            ReceiverImpl receiver = transportReceiver.getReceiver();
            Binary deliveryTag = transfer.getDeliveryTag();
            delivery = _unsettledIncomingDeliveriesById.get(_currentDeliveryId);
            delivery.getTransportDelivery().incrementSessionSize();

        }
        else
        {
            // TODO - check deliveryId has been incremented by one
            _currentDeliveryId = transfer.getDeliveryId();
            // TODO - check link handle valid and a receiver
            TransportReceiver transportReceiver = (TransportReceiver) getLinkFromRemoteHandle(transfer.getHandle());
            ReceiverImpl receiver = transportReceiver.getReceiver();
            Binary deliveryTag = transfer.getDeliveryTag();
            delivery = receiver.delivery(deliveryTag.getArray(), deliveryTag.getArrayOffset(),
                                                      deliveryTag.getLength());
            TransportDelivery transportDelivery = new TransportDelivery(_currentDeliveryId, delivery, transportReceiver);
            delivery.setTransportDelivery(transportDelivery);
            _unsettledIncomingDeliveriesById.put(_currentDeliveryId, delivery);


        }
        _unsettledIncomingSize++;
        // TODO - should this be a copy?
        if(payload != null)
        {
            if(delivery.getDataLength() == 0)
            {
                delivery.setData(payload.getArray());
                delivery.setDataLength(payload.getLength());
                delivery.setDataOffset(payload.getArrayOffset());
            }
            else
            {
                byte[] data = new byte[delivery.getDataLength() + payload.getLength()];
                System.arraycopy(delivery.getData(), delivery.getDataOffset(), data, 0, delivery.getDataLength());
                System.arraycopy(payload.getArray(), payload.getArrayOffset(), data, delivery.getDataLength(), payload.getLength());
                delivery.setData(data);
                delivery.setDataOffset(0);
                delivery.setDataLength(data.length);
            }
        }
        delivery.addIOWork();


        if(!(transfer.getMore() || transfer.getAborted()))
        {
            delivery.setComplete();
            _incomingWindowSize = _incomingWindowSize.subtract(UnsignedInteger.ONE);
            delivery.getLink().getTransportLink().decrementLinkCredit();
            delivery.getLink().getTransportLink().incrementDeliveryCount();
        }
        if(Boolean.TRUE == transfer.getSettled())
        {
            delivery.setRemoteSettled(true);
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
        if(flow.getNextIncomingId() != null)
        {
            setRemoteNextIncomingId(flow.getNextIncomingId());
        }
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
        UnsignedInteger id = disposition.getFirst();
        UnsignedInteger last = disposition.getLast() == null ? id : disposition.getLast();
        final Map<UnsignedInteger, DeliveryImpl> unsettledDeliveries =
                disposition.getRole() ? _unsettledOutgoingDeliveriesById
                        : _unsettledIncomingDeliveriesById;

        while(id.compareTo(last)<=0)
        {
            DeliveryImpl delivery = unsettledDeliveries.get(id);
            if(delivery != null)
            {
                if(disposition.getState() != null)
                {
                    delivery.setRemoteDeliveryState(disposition.getState());
                }
                if(Boolean.TRUE.equals(disposition.getSettled()))
                {
                    delivery.setRemoteSettled(true);

                }
                delivery.addToWorkList();
            }
            id = id.add(UnsignedInteger.ONE);
        }
        //TODO - Implement.
    }

    void addUnsettledOutgoing(UnsignedInteger deliveryId, DeliveryImpl delivery)
    {
        _unsettledOutgoingDeliveriesById.put(deliveryId, delivery);
        _outgoingWindowSize = _outgoingWindowSize.subtract(UnsignedInteger.valueOf(delivery.getTransportDelivery().getSessionSize()));
    }

    public boolean hasOutgoingCredit()
    {
        return _remoteIncomingWindow == null
                   ? false
                   : _remoteIncomingWindow.add(_remoteNextIncomingId).compareTo(_nextOutgoingId)>0
                     && _outgoingWindowSize.compareTo(UnsignedInteger.ZERO)>0;

    }

    void incrementOutgoingId()
    {
        _nextOutgoingId = _nextOutgoingId.add(UnsignedInteger.ONE);
    }

    public void settled(TransportDelivery transportDelivery)
    {
        if(transportDelivery.getTransportLink().getLink() instanceof ReceiverImpl)
        {
            _incomingWindowSize = _incomingWindowSize.add( UnsignedInteger.valueOf(transportDelivery.getSessionSize()));
            _incomingWindowSizeChange = true;
            getSession().modified();
        }
        else
        {
            _outgoingWindowSize = _outgoingWindowSize.add(UnsignedInteger.valueOf(transportDelivery.getSessionSize()));
            _outgoingWindowSizeChange = true;
            getSession().modified();
        }
    }

    public boolean clearIncomingWindowResize()
    {
        if(_incomingWindowSizeChange)
        {
            _incomingWindowSizeChange = false;
            return true;
        }
        return false;
    }

    public boolean clearOutgoingWindowResize()
    {
        if(_outgoingWindowSizeChange)
        {
            _outgoingWindowSizeChange = false;
            return true;
        }
        return false;
    }


    public UnsignedInteger getNextIncomingId()
    {
        return _nextIncomingId;
    }

    public void setNextIncomingId(UnsignedInteger nextIncomingId)
    {
        _nextIncomingId = nextIncomingId;
    }

    public void incrementNextIncomingId()
    {
        _nextIncomingId = _nextIncomingId.add(UnsignedInteger.ONE);
    }

    public boolean endReceived()
    {
        return _endReceived;
    }

    public void receivedEnd()
    {
        _endReceived = true;
    }

    public boolean beginSent()
    {
        return _beginSent;
    }

    public void sentBegin()
    {
        _beginSent = true;
    }
}
