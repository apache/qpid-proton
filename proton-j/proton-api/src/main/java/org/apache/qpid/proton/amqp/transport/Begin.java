
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
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.UnsignedInteger;
import org.apache.qpid.proton.amqp.UnsignedShort;

import java.util.Arrays;
import java.util.Map;


public final class Begin implements FrameBody
{

    private UnsignedShort _remoteChannel;
    private UnsignedInteger _nextOutgoingId;
    private UnsignedInteger _incomingWindow;
    private UnsignedInteger _outgoingWindow;
    private UnsignedInteger _handleMax = UnsignedInteger.valueOf(0xffffffff);
    private Symbol[] _offeredCapabilities;
    private Symbol[] _desiredCapabilities;
    private Map _properties;

    public UnsignedShort getRemoteChannel()
    {
        return _remoteChannel;
    }

    public void setRemoteChannel(UnsignedShort remoteChannel)
    {
        _remoteChannel = remoteChannel;
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

    public UnsignedInteger getHandleMax()
    {
        return _handleMax;
    }

    public void setHandleMax(UnsignedInteger handleMax)
    {
        _handleMax = handleMax;
    }

    public Symbol[] getOfferedCapabilities()
    {
        return _offeredCapabilities;
    }

    public void setOfferedCapabilities(Symbol... offeredCapabilities)
    {
        _offeredCapabilities = offeredCapabilities;
    }

    public Symbol[] getDesiredCapabilities()
    {
        return _desiredCapabilities;
    }

    public void setDesiredCapabilities(Symbol... desiredCapabilities)
    {
        _desiredCapabilities = desiredCapabilities;
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
        handler.handleBegin(this, payload, context);
    }


    @Override
    public String toString()
    {
        return "Begin{" +
               "remoteChannel=" + _remoteChannel +
               ", nextOutgoingId=" + _nextOutgoingId +
               ", incomingWindow=" + _incomingWindow +
               ", outgoingWindow=" + _outgoingWindow +
               ", handleMax=" + _handleMax +
               ", offeredCapabilities=" + (_offeredCapabilities == null ? null : Arrays.asList(_offeredCapabilities)) +
               ", desiredCapabilities=" + (_desiredCapabilities == null ? null : Arrays.asList(_desiredCapabilities)) +
               ", properties=" + _properties +
               '}';
    }
}
  