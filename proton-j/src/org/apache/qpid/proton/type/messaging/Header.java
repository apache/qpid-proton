
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


package org.apache.qpid.proton.type.messaging;
import java.util.List;
import java.util.AbstractList;


import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.type.*;


public class Header
      implements DescribedType , Section
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000070L), Symbol.valueOf("amqp:header:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000070L);
    private final HeaderWrapper _wrapper = new HeaderWrapper();

    private Boolean _durable;
    private UnsignedByte _priority;
    private UnsignedInteger _ttl;
    private Boolean _firstAcquirer;
    private UnsignedInteger _deliveryCount;

    public Boolean getDurable()
    {
        return _durable;
    }

    public void setDurable(Boolean durable)
    {
        _durable = durable;
    }

    public UnsignedByte getPriority()
    {
        return _priority;
    }

    public void setPriority(UnsignedByte priority)
    {
        _priority = priority;
    }

    public UnsignedInteger getTtl()
    {
        return _ttl;
    }

    public void setTtl(UnsignedInteger ttl)
    {
        _ttl = ttl;
    }

    public Boolean getFirstAcquirer()
    {
        return _firstAcquirer;
    }

    public void setFirstAcquirer(Boolean firstAcquirer)
    {
        _firstAcquirer = firstAcquirer;
    }

    public UnsignedInteger getDeliveryCount()
    {
        return _deliveryCount;
    }

    public void setDeliveryCount(UnsignedInteger deliveryCount)
    {
        _deliveryCount = deliveryCount;
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
                return _durable;
            case 1:
                return _priority;
            case 2:
                return _ttl;
            case 3:
                return _firstAcquirer;
            case 4:
                return _deliveryCount;
        }

        throw new IllegalStateException("Unknown index " + index);

    }

    public int size()
    {
        return _deliveryCount != null
                  ? 5
                  : _firstAcquirer != null
                  ? 4
                  : _ttl != null
                  ? 3
                  : _priority != null
                  ? 2
                  : _durable != null
                  ? 1
                  : 0;

    }


    public final class HeaderWrapper extends AbstractList
    {

        @Override
        public Object get(final int index)
        {
            return Header.this.get(index);
        }

        @Override
        public int size()
        {
            return Header.this.size();
        }
    }

    private static class HeaderConstructor implements DescribedTypeConstructor<Header>
    {
        public Header newInstance(Object described)
        {
            List l = (List) described;

            Header o = new Header();


            switch(5 - l.size())
            {

                case 0:
                    o.setDeliveryCount( (UnsignedInteger) l.get( 4 ) );
                case 1:
                    o.setFirstAcquirer( (Boolean) l.get( 3 ) );
                case 2:
                    o.setTtl( (UnsignedInteger) l.get( 2 ) );
                case 3:
                    o.setPriority( (UnsignedByte) l.get( 1 ) );
                case 4:
                    o.setDurable( (Boolean) l.get( 0 ) );
            }


            return o;
        }

        public Class<Header> getTypeClass()
        {
            return Header.class;
        }
    }


    public static void register(Decoder decoder)
    {
        HeaderConstructor constructor = new HeaderConstructor();
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, constructor);
        }
    }

    @Override
    public String toString()
    {
        return "Header{" +
               "durable=" + _durable +
               ", priority=" + _priority +
               ", ttl=" + _ttl +
               ", firstAcquirer=" + _firstAcquirer +
               ", deliveryCount=" + _deliveryCount +
               '}';
    }
}
