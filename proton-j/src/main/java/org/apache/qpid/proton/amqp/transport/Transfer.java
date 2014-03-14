
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


package org.apache.qpid.proton.amqp.transport;

import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.UnsignedInteger;

public final class Transfer implements FrameBody
{
    private UnsignedInteger _handle;
    private UnsignedInteger _deliveryId;
    private Binary _deliveryTag;
    private UnsignedInteger _messageFormat;
    private Boolean _settled;
    private boolean _more;
    private ReceiverSettleMode _rcvSettleMode;
    private DeliveryState _state;
    private boolean _resume;
    private boolean _aborted;
    private boolean _batchable;

    public UnsignedInteger getHandle()
    {
        return _handle;
    }

    public void setHandle(UnsignedInteger handle)
    {
        if( handle == null )
        {
            throw new NullPointerException("the handle field is mandatory");
        }

        _handle = handle;
    }

    public UnsignedInteger getDeliveryId()
    {
        return _deliveryId;
    }

    public void setDeliveryId(UnsignedInteger deliveryId)
    {
        _deliveryId = deliveryId;
    }

    public Binary getDeliveryTag()
    {
        return _deliveryTag;
    }

    public void setDeliveryTag(Binary deliveryTag)
    {
        _deliveryTag = deliveryTag;
    }

    public UnsignedInteger getMessageFormat()
    {
        return _messageFormat;
    }

    public void setMessageFormat(UnsignedInteger messageFormat)
    {
        _messageFormat = messageFormat;
    }

    public Boolean getSettled()
    {
        return _settled;
    }

    public void setSettled(Boolean settled)
    {
        _settled = settled;
    }

    public boolean getMore()
    {
        return _more;
    }

    public void setMore(boolean more)
    {
        _more = more;
    }

    public ReceiverSettleMode getRcvSettleMode()
    {
        return _rcvSettleMode;
    }

    public void setRcvSettleMode(ReceiverSettleMode rcvSettleMode)
    {
        _rcvSettleMode = rcvSettleMode;
    }

    public DeliveryState getState()
    {
        return _state;
    }

    public void setState(DeliveryState state)
    {
        _state = state;
    }

    public boolean getResume()
    {
        return _resume;
    }

    public void setResume(boolean resume)
    {
        _resume = resume;
    }

    public boolean getAborted()
    {
        return _aborted;
    }

    public void setAborted(boolean aborted)
    {
        _aborted = aborted;
    }

    public boolean getBatchable()
    {
        return _batchable;
    }

    public void setBatchable(boolean batchable)
    {
        _batchable = batchable;
    }

    public <E> void invoke(FrameBodyHandler<E> handler, Binary payload, E context)
    {
        handler.handleTransfer(this, payload, context);
    }

    @Override
    public String toString()
    {
        return "Transfer{" +
               "handle=" + _handle +
               ", deliveryId=" + _deliveryId +
               ", deliveryTag=" + _deliveryTag +
               ", messageFormat=" + _messageFormat +
               ", settled=" + _settled +
               ", more=" + _more +
               ", rcvSettleMode=" + _rcvSettleMode +
               ", state=" + _state +
               ", resume=" + _resume +
               ", aborted=" + _aborted +
               ", batchable=" + _batchable +
               '}';
    }
}
  