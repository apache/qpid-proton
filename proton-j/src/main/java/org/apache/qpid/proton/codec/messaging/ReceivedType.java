
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
import org.apache.qpid.proton.amqp.UnsignedInteger;
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.messaging.Received;
import org.apache.qpid.proton.codec.AbstractDescribedType;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.codec.EncoderImpl;


public final class ReceivedType extends AbstractDescribedType<Received,List> implements DescribedTypeConstructor<Received>
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000023L), Symbol.valueOf("amqp:received:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000023L);

    private ReceivedType(EncoderImpl encoder)
    {
        super(encoder);
    }

    public UnsignedLong getDescriptor()
    {
        return DESCRIPTOR;
    }

    @Override
    protected List wrap(Received val)
    {
        return new ReceivedWrapper(val);
    }


    private static final class ReceivedWrapper extends AbstractList
    {
        private final Received _impl;

        private ReceivedWrapper(Received impl)
        {
            _impl = impl;
        }

        public Object get(final int index)
        {

            switch(index)
            {
                case 0:
                    return _impl.getSectionNumber();
                case 1:
                    return _impl.getSectionOffset();
            }

            throw new IllegalStateException("Unknown index " + index);

        }

        public int size()
        {
            return _impl.getSectionOffset() != null
                      ? 2
                      : _impl.getSectionOffset() != null
                      ? 1
                      : 0;

        }
    }

    public Received newInstance(Object described)
    {
        List l = (List) described;

        Received o = new Received();


        switch(2 - l.size())
        {

            case 0:
                o.setSectionOffset( (UnsignedLong) l.get( 1 ) );
            case 1:
                o.setSectionNumber( (UnsignedInteger) l.get( 0 ) );
        }


        return o;
    }

    public Class<Received> getTypeClass()
    {
        return Received.class;
    }


    public static void register(Decoder decoder, EncoderImpl encoder)
    {
        ReceivedType type = new ReceivedType(encoder);
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, type);
        }
        encoder.register(type);
    }
}
  