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

package org.apache.qpid.proton.transport2;

import java.util.List;

import org.apache.qpid.proton.codec2.DecodeException;
import org.apache.qpid.proton.codec2.DescribedTypeFactory;
import org.apache.qpid.proton.codec2.Encodable;
import org.apache.qpid.proton.codec2.Encoder;

public final class Disposition implements Encodable, Performative
{
    public final static long CODE = 0x0000000000000015L;

    public final static String DESCRIPTOR = "amqp:disposition:list";

    public final static Factory FACTORY = new Factory();

    private Role _role = Role.SENDER;

    private int _first;

    private int _last;

    private boolean _settled;

    private DeliveryState _state;

    private boolean _batchable;

    public Role getRole()
    {
        return _role;
    }

    public void setRole(Role role)
    {
        if (role == null)
        {
            throw new NullPointerException("Role cannot be null");
        }
        _role = role;
    }

    public int getFirst()
    {
        return _first;
    }

    public void setFirst(int first)
    {
        _first = first;
    }

    public int getLast()
    {
        return _last;
    }

    public void setLast(int last)
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

    @Override
    public void encode(Encoder encoder)
    {
        encoder.putDescriptor();
        encoder.putUlong(CODE);
        encoder.putList();
        encoder.putBoolean(_role.getValue());
        encoder.putUint(_first);
        encoder.putUint(_last);
        encoder.putBoolean(_settled);
        if (_state == null)
        {
            encoder.putNull();
        }
        else
        {
            _state.encode(encoder);
        }
        encoder.end();
    }

    public static final class Factory implements DescribedTypeFactory
    {
        @SuppressWarnings("unchecked")
        public Object create(Object in) throws DecodeException
        {
            List<Object> l = (List<Object>) in;
            Disposition disposition = new Disposition();

            if (l.isEmpty())
            {
                throw new DecodeException("The first field cannot be omitted");
            }

            switch (6 - l.size())
            {

            case 0:
                disposition.setBatchable(l.get(5) == null ? false : (Boolean) l.get(5));
            case 1:
                disposition.setState((DeliveryState) l.get(4));
            case 2:
                disposition.setSettled(l.get(3) == null ? false : (Boolean) l.get(3));
            case 3:
                disposition.setLast((Integer) l.get(2));
            case 4:
                disposition.setFirst((Integer) l.get(1));
            case 5:
                disposition.setRole(Boolean.TRUE.equals(l.get(0)) ? Role.RECEIVER : Role.SENDER);
            }
            return disposition;
        }
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

    @Override
    public long getCode()
    {
        return CODE;
    }

    @Override
    public String getDescriptor()
    {
        return DESCRIPTOR;
    }
}