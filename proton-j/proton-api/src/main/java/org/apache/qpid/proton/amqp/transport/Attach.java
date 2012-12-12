
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
import org.apache.qpid.proton.amqp.UnsignedLong;

import java.util.Arrays;
import java.util.Map;

public final class Attach implements FrameBody
{

    private String _name;
    private UnsignedInteger _handle;
    private Role _role = Role.SENDER;
    private SenderSettleMode _sndSettleMode = SenderSettleMode.MIXED;
    private ReceiverSettleMode _rcvSettleMode = ReceiverSettleMode.FIRST;
    private Source _source;
    private Target _target;
    private Map _unsettled;
    private boolean _incompleteUnsettled;
    private UnsignedInteger _initialDeliveryCount;
    private UnsignedLong _maxMessageSize;
    private Symbol[] _offeredCapabilities;
    private Symbol[] _desiredCapabilities;
    private Map _properties;

    public String getName()
    {
        return _name;
    }

    public void setName(String name)
    {
        if( name == null )
        {
            throw new NullPointerException("the name field is mandatory");
        }

        _name = name;
    }

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

    public Role getRole()
    {
        return _role;
    }

    public void setRole(Role role)
    {
        if(role == null)
        {
            throw new NullPointerException("Role cannot be null");
        }
        _role = role;
    }

    public SenderSettleMode getSndSettleMode()
    {
        return _sndSettleMode;
    }

    public void setSndSettleMode(SenderSettleMode sndSettleMode)
    {
        _sndSettleMode = sndSettleMode == null ? SenderSettleMode.MIXED : sndSettleMode;
    }

    public ReceiverSettleMode getRcvSettleMode()
    {
        return _rcvSettleMode;
    }

    public void setRcvSettleMode(ReceiverSettleMode rcvSettleMode)
    {
        _rcvSettleMode = rcvSettleMode == null ? ReceiverSettleMode.FIRST : rcvSettleMode;
    }

    public Source getSource()
    {
        return _source;
    }

    public void setSource(Source source)
    {
        _source = source;
    }

    public Target getTarget()
    {
        return _target;
    }

    public void setTarget(Target target)
    {
        _target = target;
    }

    public Map getUnsettled()
    {
        return _unsettled;
    }

    public void setUnsettled(Map unsettled)
    {
        _unsettled = unsettled;
    }

    public boolean getIncompleteUnsettled()
    {
        return _incompleteUnsettled;
    }

    public void setIncompleteUnsettled(boolean incompleteUnsettled)
    {
        _incompleteUnsettled = incompleteUnsettled;
    }

    public UnsignedInteger getInitialDeliveryCount()
    {
        return _initialDeliveryCount;
    }

    public void setInitialDeliveryCount(UnsignedInteger initialDeliveryCount)
    {
        _initialDeliveryCount = initialDeliveryCount;
    }

    public UnsignedLong getMaxMessageSize()
    {
        return _maxMessageSize;
    }

    public void setMaxMessageSize(UnsignedLong maxMessageSize)
    {
        _maxMessageSize = maxMessageSize;
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
        handler.handleAttach(this, payload, context);
    }


    @Override
    public String toString()
    {
        return "Attach{" +
               "name='" + _name + '\'' +
               ", handle=" + _handle +
               ", role=" + _role +
               ", sndSettleMode=" + _sndSettleMode +
               ", rcvSettleMode=" + _rcvSettleMode +
               ", source=" + _source +
               ", target=" + _target +
               ", unsettled=" + _unsettled +
               ", incompleteUnsettled=" + _incompleteUnsettled +
               ", initialDeliveryCount=" + _initialDeliveryCount +
               ", maxMessageSize=" + _maxMessageSize +
               ", offeredCapabilities=" + (_offeredCapabilities == null ? null : Arrays.asList(_offeredCapabilities)) +
               ", desiredCapabilities=" + (_desiredCapabilities == null ? null : Arrays.asList(_desiredCapabilities)) +
               ", properties=" + _properties +
               '}';
    }
}
