
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

public final class Disposition implements FrameBody
{
    private Role _role = Role.SENDER;
    private UnsignedInteger _first;
    private UnsignedInteger _last;
    private boolean _settled;
    private DeliveryState _state;
    private boolean _batchable;

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

    public UnsignedInteger getFirst()
    {
        return _first;
    }

    public void setFirst(UnsignedInteger first)
    {
        if( first == null )
        {
            throw new NullPointerException("the first field is mandatory");
        }

        _first = first;
    }

    public UnsignedInteger getLast()
    {
        return _last;
    }

    public void setLast(UnsignedInteger last)
    {
        _last = last;
    }

    public boolean getSettled()
    {
        return _settled;
    }

    public void setSettled(boolean settled)
    {
        _settled = settled;
    }

    public DeliveryState getState()
    {
        return _state;
    }

    public void setState(DeliveryState state)
    {
        _state = state;
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
        handler.handleDisposition(this, payload, context);
    }

    @Override
    public String toString()
    {
        return "Disposition{" +
               "role=" + _role +
               ", first=" + _first +
               ", last=" + _last +
               ", settled=" + _settled +
               ", state=" + _state +
               ", batchable=" + _batchable +
               '}';
    }
}
