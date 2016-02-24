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

import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.UnsignedInteger;
import org.apache.qpid.proton.amqp.transport.Disposition;
import org.apache.qpid.proton.amqp.transport.Flow;
import org.apache.qpid.proton.amqp.transport.Role;
import org.apache.qpid.proton.amqp.transport.Transfer;
import org.apache.qpid.proton.engine.Event;

class TransportSession
{
    private static final int HANDLE_MAX = 65535;

    private final TransportImpl _transport;
    private final SessionImpl _session;
    private int _localChannel = -1;
    private int _remoteChannel = -1;
    private boolean _openSent;
    private final UnsignedInteger _handleMax = UnsignedInteger.valueOf(HANDLE_MAX); //TODO: should this be configurable?
    // This is used for the delivery-id actually stamped in each transfer frame of a given message delivery.
    private UnsignedInteger _outgoingDeliveryId = UnsignedInteger.ZERO;
    // These are used for the session windows communicated via Begin/Flow frames
    // and the conceptual transfer-id relating to updating them.
    private UnsignedInteger _incomingWindowSize = UnsignedInteger.ZERO;
    private UnsignedInteger _outgoingWindowSize = UnsignedInteger.ZERO;
    private UnsignedInteger _nextOutgoingId = UnsignedInteger.ONE;
    private UnsignedInteger _nextIncomingId = null;

    private final Map<UnsignedInteger, TransportLink<?>> _remoteHandlesMap = new HashMap<UnsignedInteger, TransportLink<?>>();
    private final Map<UnsignedInteger, TransportLink<?>> _localHandlesMap = new HashMap<UnsignedInteger, TransportLink<?>>();
    private final Map<String, TransportLink> _halfOpenLinks = new HashMap<String, TransportLink>();


    private UnsignedInteger _incomingDeliveryId = null;
    private UnsignedInteger _remoteIncomingWindow;
    private UnsignedInteger _remoteOutgoingWindow;
    private UnsignedInteger _remoteNextIncomingId = _nextOutgoingId;
    private UnsignedInteger _remoteNextOutgoingId;
    private final Map<UnsignedInteger, DeliveryImpl>
            _unsettledIncomingDeliveriesById = new HashMap<UnsignedInteger, DeliveryImpl>();
    private final Map<UnsignedInteger, DeliveryImpl>
            _unsettledOutgoingDeliveriesById = new HashMap<UnsignedInteger, DeliveryImpl>();
    private int _unsettledIncomingSize;
    private boolean _endReceived;
    private boolean _beginSent;

    TransportSession(TransportImpl transport, SessionImpl session)
    {
        _transport = transport;
        _session = session;
        _outgoingWindowSize = UnsignedInteger.valueOf(session.getOutgoingWindow());
    }

    void unbind()
    {
        unsetLocalChannel();
        unsetRemoteChannel();
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
        if (!isLocalChannelSet()) {
            _session.incref();
        }
        _localChannel = localChannel;
    }

    public int getRemoteChannel()
    {
        return _remoteChannel;
    }

    public void setRemoteChannel(int remoteChannel)
    {
        if (!isRemoteChannelSet()) {
            _session.incref();
        }
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
        if (isLocalChannelSet()) {
            unsetLocalHandles();
            _session.decref();
        }
        _localChannel = -1;
    }

    private void unsetLocalHandles()
    {
        for (TransportLink<?> tl : _localHandlesMap.values())
        {
            tl.clearLocalHandle();
        }
        _localHandlesMap.clear();
    }

    public void unsetRemoteChannel()
    {
        if (isRemoteChannelSet()) {
            unsetRemoteHandles();
            _session.decref();
        }
        _remoteChannel = -1;
    }

    private void unsetRemoteHandles()
    {
        for (TransportLink<?> tl : _remoteHandlesMap.values())
        {
            tl.clearRemoteHandle();
        }
        _remoteHandlesMap.clear();
    }

    public UnsignedInteger getHandleMax()
    {
        return _handleMax;
    }

    public UnsignedInteger getIncomingWindowSize()
    {
        return _incomingWindowSize;
    }

    void updateIncomingWindow()
    {
        int size = _transport.getMaxFrameSize();
        if (size <= 0) {
            _incomingWindowSize = UnsignedInteger.valueOf(2147483647); // biggest legal value
        } else {
            _incomingWindowSize = UnsignedInteger.valueOf((_session.getIncomingCapacity() - _session.getIncomingBytes())/size);
        }
    }

    public UnsignedInteger getOutgoingDeliveryId()
    {
        return _outgoingDeliveryId;
    }

    void incrementOutgoingDeliveryId()
    {
        _outgoingDeliveryId = _outgoingDeliveryId.add(UnsignedInteger.ONE);
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
        return _remoteHandlesMap.get(handle);
    }

    public UnsignedInteger allocateLocalHandle(TransportLink transportLink)
    {
        for(int i = 0; i <= HANDLE_MAX; i++)
        {
            UnsignedInteger handle = UnsignedInteger.valueOf(i);
            if(!_localHandlesMap.containsKey(handle))
            {
                _localHandlesMap.put(handle, transportLink);
                transportLink.setLocalHandle(handle);
                return handle;
            }
        }
        throw new IllegalStateException("no local handle available for allocation");
    }

    public void addLinkRemoteHandle(TransportLink link, UnsignedInteger remoteHandle)
    {
        _remoteHandlesMap.put(remoteHandle, link);
    }

    public void addLinkLocalHandle(TransportLink link, UnsignedInteger localhandle)
    {
        _localHandlesMap.put(localhandle, link);
    }

    public void freeLocalHandle(UnsignedInteger handle)
    {
        _localHandlesMap.remove(handle);
    }

    public void freeRemoteHandle(UnsignedInteger handle)
    {
        _remoteHandlesMap.remove(handle);
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
        if(transfer.getDeliveryId() == null || transfer.getDeliveryId().equals(_incomingDeliveryId))
        {
            TransportReceiver transportReceiver = (TransportReceiver) getLinkFromRemoteHandle(transfer.getHandle());
            ReceiverImpl receiver = transportReceiver.getReceiver();
            Binary deliveryTag = transfer.getDeliveryTag();
            delivery = _unsettledIncomingDeliveriesById.get(_incomingDeliveryId);
            delivery.getTransportDelivery().incrementSessionSize();

        }
        else
        {
            // TODO - check deliveryId has been incremented by one
            _incomingDeliveryId = transfer.getDeliveryId();
            // TODO - check link handle valid and a receiver
            TransportReceiver transportReceiver = (TransportReceiver) getLinkFromRemoteHandle(transfer.getHandle());
            ReceiverImpl receiver = transportReceiver.getReceiver();
            Binary deliveryTag = transfer.getDeliveryTag();
            delivery = receiver.delivery(deliveryTag.getArray(), deliveryTag.getArrayOffset(),
                                                      deliveryTag.getLength());
            UnsignedInteger messageFormat = transfer.getMessageFormat();
            if(messageFormat != null) {
                delivery.setMessageFormat(messageFormat.intValue());
            }
            TransportDelivery transportDelivery = new TransportDelivery(_incomingDeliveryId, delivery, transportReceiver);
            delivery.setTransportDelivery(transportDelivery);
            _unsettledIncomingDeliveriesById.put(_incomingDeliveryId, delivery);
            getSession().incrementIncomingDeliveries(1);
        }
        if( transfer.getState()!=null )
        {
            delivery.setRemoteDeliveryState(transfer.getState());
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
            getSession().incrementIncomingBytes(payload.getLength());
        }
        delivery.updateWork();


        if(!(transfer.getMore() || transfer.getAborted()))
        {
            delivery.setComplete();
            delivery.getLink().getTransportLink().decrementLinkCredit();
            delivery.getLink().getTransportLink().incrementDeliveryCount();
        }
        if(Boolean.TRUE.equals(transfer.getSettled()))
        {
            delivery.setRemoteSettled(true);
        }

        _incomingWindowSize = _incomingWindowSize.subtract(UnsignedInteger.ONE);

        // this will cause a flow to happen
        if (_incomingWindowSize.equals(UnsignedInteger.ZERO)) {
            delivery.getLink().modified(false);
        }

        getSession().getConnection().put(Event.Type.DELIVERY, delivery);
    }

    public void freeLocalChannel()
    {
        unsetLocalChannel();
    }

    public void freeRemoteChannel()
    {
        unsetRemoteChannel();
    }

    private void setRemoteIncomingWindow(UnsignedInteger incomingWindow)
    {
        _remoteIncomingWindow = incomingWindow;
    }

    void decrementRemoteIncomingWindow()
    {
        _remoteIncomingWindow = _remoteIncomingWindow.subtract(UnsignedInteger.ONE);
    }

    private void setRemoteOutgoingWindow(UnsignedInteger outgoingWindow)
    {
        _remoteOutgoingWindow = outgoingWindow;
    }

    void handleFlow(Flow flow)
    {
        UnsignedInteger inext = flow.getNextIncomingId();
        UnsignedInteger iwin = flow.getIncomingWindow();

        if(inext != null)
        {
            setRemoteNextIncomingId(inext);
            setRemoteIncomingWindow(inext.add(iwin).subtract(_nextOutgoingId));
        }
        else
        {
            setRemoteIncomingWindow(iwin);
        }
        setRemoteNextOutgoingId(flow.getNextOutgoingId());
        setRemoteOutgoingWindow(flow.getOutgoingWindow());

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
                disposition.getRole() == Role.RECEIVER ? _unsettledOutgoingDeliveriesById
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
                    unsettledDeliveries.remove(id);
                }
                delivery.updateWork();

                getSession().getConnection().put(Event.Type.DELIVERY, delivery);
            }
            id = id.add(UnsignedInteger.ONE);
        }
        //TODO - Implement.
    }

    void addUnsettledOutgoing(UnsignedInteger deliveryId, DeliveryImpl delivery)
    {
        _unsettledOutgoingDeliveriesById.put(deliveryId, delivery);
    }

    public boolean hasOutgoingCredit()
    {
        return _remoteIncomingWindow == null ? false
            : _remoteIncomingWindow.compareTo(UnsignedInteger.ZERO)>0;

    }

    void incrementOutgoingId()
    {
        _nextOutgoingId = _nextOutgoingId.add(UnsignedInteger.ONE);
    }

    public void settled(TransportDelivery transportDelivery)
    {
        if(transportDelivery.getTransportLink().getLink() instanceof ReceiverImpl)
        {
            _unsettledIncomingDeliveriesById.remove(transportDelivery.getDeliveryId());
            getSession().modified(false);
        }
        else
        {
            _unsettledOutgoingDeliveriesById.remove(transportDelivery.getDeliveryId());
            getSession().modified(false);
        }
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
