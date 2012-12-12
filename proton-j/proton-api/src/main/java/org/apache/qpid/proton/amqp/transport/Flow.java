
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

import java.util.Map;

import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.UnsignedInteger;

public final class Flow implements FrameBody
{
    private UnsignedInteger _nextIncomingId;
    private UnsignedInteger _incomingWindow;
    private UnsignedInteger _nextOutgoingId;
    private UnsignedInteger _outgoingWindow;
    private UnsignedInteger _handle;
    private UnsignedInteger _deliveryCount;
    private UnsignedInteger _linkCredit;
    private UnsignedInteger _available;
    private boolean _drain;
    private boolean _echo;
    private Map _properties;

    public UnsignedInteger getNextIncomingId()
    {
        return _nextIncomingId;
    }

    public void setNextIncomingId(UnsignedInteger nextIncomingId)
    {
        _nextIncomingId = nextIncomingId;
    }

    public UnsignedInteger getIncomingWindow()
    {
        return _incomingWindow;
    }

    public void setIncomingWindow(UnsignedInteger incomingWindow)
    {
        if( incomingWindow == null )
        {
            throw new NullPointerException("the incoming-window field is mandatory");
        }

        _incomingWindow = incomingWindow;
    }

    public UnsignedInteger getNextOutgoingId()
    {
        return _nextOutgoingId;
    }

    public void setNextOutgoingId(UnsignedInteger nextOutgoingId)
    {
        if( nextOutgoingId == null )
        {
            throw new NullPointerException("the next-outgoing-id field is mandatory");
        }

        _nextOutgoingId = nextOutgoingId;
    }

    public UnsignedInteger getOutgoingWindow()
    {
        return _outgoingWindow;
    }

    public void setOutgoingWindow(UnsignedInteger outgoingWindow)
    {
        if( outgoingWindow == null )
        {
            throw new NullPointerException("the outgoing-window field is mandatory");
        }

        _outgoingWindow = outgoingWindow;
    }

    public UnsignedInteger getHandle()
    {
        return _handle;
    }

    public void setHandle(UnsignedInteger handle)
    {
        _handle = handle;
    }

    public UnsignedInteger getDeliveryCount()
    {
        return _deliveryCount;
    }

    public void setDeliveryCount(UnsignedInteger deliveryCount)
    {
        _deliveryCount = deliveryCount;
    }

    public UnsignedInteger getLinkCredit()
    {
        return _linkCredit;
    }

    public void setLinkCredit(UnsignedInteger linkCredit)
    {
        _linkCredit = linkCredit;
    }

    public UnsignedInteger getAvailable()
    {
        return _available;
    }

    public void setAvailable(UnsignedInteger available)
    {
        _available = available;
    }

    public boolean getDrain()
    {
        return _drain;
    }

    public void setDrain(boolean drain)
    {
        _drain = drain;
    }

    public boolean getEcho()
    {
        return _echo;
    }

    public void setEcho(boolean echo)
    {
        _echo = echo;
    }

    public Map getProperties()
    {
        return _properties;
    }

    public void setProperties(Map properties)
    {
        _properties = properties;
    }

    public <E> void invoke(FrameBodyHandler<E> handler, Binary payload, E context)
    {
        handler.handleFlow(this, payload, context);
    }

    @Override
    public String toString()
    {
        return "Flow{" +
               "nextIncomingId=" + _nextIncomingId +
               ", incomingWindow=" + _incomingWindow +
               ", nextOutgoingId=" + _nextOutgoingId +
               ", outgoingWindow=" + _outgoingWindow +
               ", handle=" + _handle +
               ", deliveryCount=" + _deliveryCount +
               ", linkCredit=" + _linkCredit +
               ", available=" + _available +
               ", drain=" + _drain +
               ", echo=" + _echo +
               ", properties=" + _properties +
               '}';
    }
}
  