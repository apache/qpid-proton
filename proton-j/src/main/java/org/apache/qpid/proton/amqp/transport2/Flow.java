
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

package org.apache.qpid.proton.amqp.transport2;

import java.util.Map;

public class Flow
{
    private long _nextIncomingId;
    private long _incomingWindow;
    private long _nextOutgoingId;
    private long _outgoingWindow;
    private long _handle;
    private long _deliveryCount;
    private long _linkCredit;
    private long _available;
    private boolean _drain;
    private boolean _echo;
    private Map _properties;

    public long getNextIncomingId()
    {
        return _nextIncomingId;
    }

    public void setNextIncomingId(long nextIncomingId)
    {
        _nextIncomingId = nextIncomingId;
    }

    public long getIncomingWindow()
    {
        return _incomingWindow;
    }

    public void setIncomingWindow(long incomingWindow)
    {
        _incomingWindow = incomingWindow;
    }

    public long getNextOutgoingId()
    {
        return _nextOutgoingId;
    }

    public void setNextOutgoingId(long nextOutgoingId)
    {
        _nextOutgoingId = nextOutgoingId;
    }

    public long getOutgoingWindow()
    {
        return _outgoingWindow;
    }

    public void setOutgoingWindow(long outgoingWindow)
    {
         _outgoingWindow = outgoingWindow;
    }

    public long getHandle()
    {
        return _handle;
    }

    public void setHandle(long handle)
    {
        _handle = handle;
    }

    public long getDeliveryCount()
    {
        return _deliveryCount;
    }

    public void setDeliveryCount(long deliveryCount)
    {
        _deliveryCount = deliveryCount;
    }

    public long getLinkCredit()
    {
        return _linkCredit;
    }

    public void setLinkCredit(long linkCredit)
    {
        _linkCredit = linkCredit;
    }

    public long getAvailable()
    {
        return _available;
    }

    public void setAvailable(long available)
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