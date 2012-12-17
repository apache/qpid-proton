
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


package org.apache.qpid.proton.codec.messaging;

import java.util.AbstractList;
import java.util.List;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.UnsignedByte;
import org.apache.qpid.proton.amqp.UnsignedInteger;
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.messaging.Header;
import org.apache.qpid.proton.codec.AbstractDescribedType;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.codec.EncoderImpl;


public class HeaderType extends AbstractDescribedType<Header,List> implements DescribedTypeConstructor<Header>
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000070L), Symbol.valueOf("amqp:header:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000070L);

    public HeaderType(EncoderImpl encoder)
    {
        super(encoder);
    }

    @Override
    protected UnsignedLong getDescriptor()
    {
        return DESCRIPTOR;
    }

    @Override
    protected List wrap(Header val)
    {
        return new HeaderWrapper(val);
    }


    public final class HeaderWrapper extends AbstractList
    {
        private final Header _impl;

        public HeaderWrapper(Header impl)
        {
            _impl = impl;
        }


        @Override
        public Object get(final int index)
            {

                switch(index)
                {
                    case 0:
                        return _impl.getDurable();
                    case 1:
                        return _impl.getPriority();
                    case 2:
                        return _impl.getTtl();
                    case 3:
                        return _impl.getFirstAcquirer();
                    case 4:
                        return _impl.getDeliveryCount();
                }

                throw new IllegalStateException("Unknown index " + index);

            }

            public int size()
            {
                return _impl.getDeliveryCount() != null
                          ? 5
                          : _impl.getFirstAcquirer() != null
                          ? 4
                          : _impl.getTtl() != null
                          ? 3
                          : _impl.getPriority() != null
                          ? 2
                          : _impl.getDurable() != null
                          ? 1
                          : 0;

            }


    }

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

    public static void register(Decoder decoder, EncoderImpl encoder)
    {
        HeaderType type = new HeaderType(encoder);
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, type);
        }
        encoder.register(type);
    }

}
