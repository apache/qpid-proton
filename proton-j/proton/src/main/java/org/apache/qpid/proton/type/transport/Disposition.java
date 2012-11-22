
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


package org.apache.qpid.proton.type.transport;
import java.util.List;
import java.util.AbstractList;


import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.type.*;


public class Disposition
      implements DescribedType , FrameBody
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000015L), Symbol.valueOf("amqp:disposition:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000015L);
    private final DispositionWrapper _wrapper = new DispositionWrapper();

    private boolean _role;
    private UnsignedInteger _first;
    private UnsignedInteger _last;
    private boolean _settled;
    private DeliveryState _state;
    private boolean _batchable;

    public boolean getRole()
    {
        return _role;
    }

    public void setRole(boolean role)
    {
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

    public Object getDescriptor()
    {
        return DESCRIPTOR;
    }

    public Object getDescribed()
    {
        return _wrapper;
    }

    public Object get(final int index)
    {

        switch(index)
        {
            case 0:
                return _role;
            case 1:
                return _first;
            case 2:
                return _last;
            case 3:
                return _settled;
            case 4:
                return _state;
            case 5:
                return _batchable;
        }

        throw new IllegalStateException("Unknown index " + index);

    }

    public int size()
    {
        return (_batchable != false)
                  ? 6
                  : _state != null
                  ? 5
                  : (_settled != false)
                  ? 4
                  : _last != null
                  ? 3
                  : 2;

    }


    public final class DispositionWrapper extends AbstractList
    {

        @Override
        public Object get(final int index)
        {
            return Disposition.this.get(index);
        }

        @Override
        public int size()
        {
            return Disposition.this.size();
        }
    }

    private static class DispositionConstructor implements DescribedTypeConstructor<Disposition>
    {
        public Disposition newInstance(Object described)
        {
            List l = (List) described;

            Disposition o = new Disposition();

            if(l.size() <= 1)
            {
                throw new DecodeException("The first field cannot be omitted");
            }

            switch(6 - l.size())
            {

                case 0:
                    Boolean batchable = (Boolean) l.get(5);
                    o.setBatchable(batchable == null ? false : batchable);
                case 1:
                    o.setState( (DeliveryState) l.get( 4 ) );
                case 2:
                    Boolean settled = (Boolean) l.get(3);
                    o.setSettled(settled == null ? false : settled);
                case 3:
                    o.setLast( (UnsignedInteger) l.get( 2 ) );
                case 4:
                    o.setFirst( (UnsignedInteger) l.get( 1 ) );
                case 5:
                    o.setRole( (Boolean) l.get( 0 ) );
            }


            return o;
        }

        public Class<Disposition> getTypeClass()
        {
            return Disposition.class;
        }
    }

    public <E> void invoke(FrameBodyHandler<E> handler, Binary payload, E context)
    {
        handler.handleDisposition(this, payload, context);
    }


    public static void register(Decoder decoder)
    {
        DispositionConstructor constructor = new DispositionConstructor();
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, constructor);
        }
    }

    @Override
    public String toString()
    {
        return "Disposition{" +
               "role=" + (_role ? "RECIEVER" : "SENDER") +
               ", first=" + _first +
               ", last=" + _last +
               ", settled=" + _settled +
               ", state=" + _state +
               ", batchable=" + _batchable +
               '}';
    }
}
