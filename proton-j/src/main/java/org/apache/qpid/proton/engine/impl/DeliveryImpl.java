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

import java.util.Arrays;

import org.apache.qpid.proton.engine.Delivery;
import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.amqp.transport.DeliveryState;

public class DeliveryImpl implements Delivery
{
    private DeliveryImpl _linkPrevious;
    private DeliveryImpl _linkNext;

    private DeliveryImpl _workNext;
    private DeliveryImpl _workPrev;
    boolean _work;

    private DeliveryImpl _transportWorkNext;
    private DeliveryImpl _transportWorkPrev;
    boolean _transportWork;

    private Object _context;

    private final byte[] _tag;
    private final LinkImpl _link;
    private DeliveryState _deliveryState;
    private boolean _settled;
    private boolean _remoteSettled;
    private DeliveryState _remoteDeliveryState;

    private static final int DELIVERY_STATE_CHANGED = 1;
    private static final int ABLE_TO_SEND = 2;
    private static final int IO_WORK = 4;
    private static final int DELIVERY_SETTLED = 8;


    /**
     * A bit-mask representing the outstanding work on this delivery received from the transport layer
     * that has not yet been processed by the application.
     */
    private int _flags = (byte) 0;

    private TransportDelivery _transportDelivery;
    private byte[] _data;
    private int _dataSize;
    private boolean _complete;
    private boolean _updated;
    private boolean _done;
    private int _offset;

    DeliveryImpl(final byte[] tag, final LinkImpl link, DeliveryImpl previous)
    {
        _tag = tag;
        _link = link;
        _link.incrementUnsettled();
        _linkPrevious = previous;
        if(previous != null)
        {
            previous._linkNext = this;
        }
    }

    public byte[] getTag()
    {
        return _tag;
    }

    public LinkImpl getLink()
    {
        return _link;
    }

    public DeliveryState getLocalState()
    {
        return _deliveryState;
    }

    public DeliveryState getRemoteState()
    {
        return _remoteDeliveryState;
    }

    public boolean remotelySettled()
    {
        return _remoteSettled;
    }

    public int getMessageFormat()
    {
        return 0;
    }

    public void disposition(final DeliveryState state)
    {
        _deliveryState = state;
        if(!_remoteSettled)
        {
            addToTransportWorkList();
        }
    }

    public void settle()
    {
        if (_settled) {
            return;
        }

        _settled = true;
        _link.decrementUnsettled();
        if(!_remoteSettled)
        {
            addToTransportWorkList();
        }
        else
        {
            _transportDelivery.settled();
        }
        if(_link.current() == this)
        {
            _link.advance();
        }

        _link.remove(this);
        if(_linkPrevious != null)
        {
            _linkPrevious._linkNext = _linkNext;
        }
        if(_linkNext != null)
        {
            _linkNext._linkPrevious = _linkPrevious;
        }
        updateWork();
    }

    DeliveryImpl getLinkNext()
    {
        return _linkNext;
    }

    public DeliveryImpl next()
    {
        return getLinkNext();
    }

    public void free()
    {

    }

    DeliveryImpl getLinkPrevious()
    {
        return _linkPrevious;
    }

    public DeliveryImpl getWorkNext()
    {
        if (_workNext != null)
            return _workNext;
        // the following hack is brought to you by the C implementation!
        if (!_work)  // not on the work list
            return _link.getConnectionImpl().getWorkHead();
        return null;
    }

    DeliveryImpl getWorkPrev()
    {
        return _workPrev;
    }


    void setWorkNext(DeliveryImpl workNext)
    {
        _workNext = workNext;
    }

    void setWorkPrev(DeliveryImpl workPrev)
    {
        _workPrev = workPrev;
    }

    int recv(byte[] bytes, int offset, int size)
    {

        final int consumed;
        if(_data != null)
        {
            //TODO - should only be if no bytes left
            consumed = Math.min(size, _dataSize);

            System.arraycopy(_data, _offset, bytes, offset, consumed);
            _offset += consumed;
            _dataSize -= consumed;
        }
        else
        {
            _dataSize =  consumed = 0;
        }
        return (_complete && consumed == 0) ? Transport.END_OF_STREAM : consumed;  //TODO - Implement
    }

    void updateWork()
    {
        getLink().getConnectionImpl().workUpdate(this);
    }

    DeliveryImpl clearTransportWork()
    {
        DeliveryImpl next = _transportWorkNext;
        getLink().getConnectionImpl().removeTransportWork(this);
        return next;
    }

    void addToTransportWorkList()
    {
        getLink().getConnectionImpl().addTransportWork(this);
    }


    DeliveryImpl getTransportWorkNext()
    {
        return _transportWorkNext;
    }


    DeliveryImpl getTransportWorkPrev()
    {
        return _transportWorkPrev;
    }

    void setTransportWorkNext(DeliveryImpl transportWorkNext)
    {
        _transportWorkNext = transportWorkNext;
    }

    void setTransportWorkPrev(DeliveryImpl transportWorkPrev)
    {
        _transportWorkPrev = transportWorkPrev;
    }

    TransportDelivery getTransportDelivery()
    {
        return _transportDelivery;
    }

    void setTransportDelivery(TransportDelivery transportDelivery)
    {
        _transportDelivery = transportDelivery;
    }

    public boolean isSettled()
    {
        return _settled;
    }

    int send(byte[] bytes, int offset, int length)
    {
        if(_data == null)
        {
            _data = new byte[length];
        }
        else if(_data.length - _dataSize < length)
        {
            byte[] oldData = _data;
            _data = new byte[oldData.length + _dataSize];
            System.arraycopy(oldData,_offset,_data,0,_dataSize);
            _offset = 0;
        }
        System.arraycopy(bytes,offset,_data,_dataSize+_offset,length);
        _dataSize+=length;
        addToTransportWorkList();
        return length;  //TODO - Implement.
    }

    byte[] getData()
    {
        return _data;
    }

    int getDataOffset()
    {
        return _offset;
    }

    int getDataLength()
    {
        return _dataSize;  //TODO - Implement.
    }

    void setData(byte[] data)
    {
        _data = data;
    }

    void setDataLength(int length)
    {
        _dataSize = length;
    }

    public void setDataOffset(int arrayOffset)
    {
        _offset = arrayOffset;
    }

    public boolean isWritable()
    {
        return getLink() instanceof SenderImpl
                && getLink().current() == this
                && ((SenderImpl) getLink()).hasCredit();
    }

    public boolean isReadable()
    {
        return getLink() instanceof ReceiverImpl
            && getLink().current() == this;
    }

    void setComplete()
    {
        _complete = true;
    }

    public boolean isPartial()
    {
        return !_complete;
    }

    void setRemoteDeliveryState(DeliveryState remoteDeliveryState)
    {
        _remoteDeliveryState = remoteDeliveryState;
        _updated = true;
    }

    public boolean isUpdated()
    {
        return _updated;
    }

    public void clear()
    {
        _updated = false;
        getLink().getConnectionImpl().workUpdate(this);
    }


    void setDone()
    {
        _done = true;
    }

    boolean isDone()
    {
        return _done;
    }

    void setRemoteSettled(boolean remoteSettled)
    {
        _remoteSettled = remoteSettled;
        _updated = true;
    }

    public boolean isBuffered()
    {
        if (_remoteSettled) return false;
        if (getLink() instanceof SenderImpl) {
            if (isDone()) {
                return false;
            } else {
                return _complete || _dataSize > 0;
            }
        } else {
            return false;
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

    @Override
    public String toString()
    {
        StringBuilder builder = new StringBuilder();
        builder.append("DeliveryImpl [_tag=").append(Arrays.toString(_tag))
            .append(", _link=").append(_link)
            .append(", _deliveryState=").append(_deliveryState)
            .append(", _settled=").append(_settled)
            .append(", _remoteSettled=").append(_remoteSettled)
            .append(", _remoteDeliveryState=").append(_remoteDeliveryState)
            .append(", _flags=").append(_flags)
            .append(", _transportDelivery=").append(_transportDelivery)
            .append(", _dataSize=").append(_dataSize)
            .append(", _complete=").append(_complete)
            .append(", _updated=").append(_updated)
            .append(", _done=").append(_done)
            .append(", _offset=").append(_offset).append("]");
        return builder.toString();
    }

    public int pending()
    {
        return _dataSize;
    }

}
