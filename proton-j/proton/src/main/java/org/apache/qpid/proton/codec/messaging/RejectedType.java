
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
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.messaging.Rejected;
import org.apache.qpid.proton.codec.AbstractDescribedType;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.codec.EncoderImpl;
import org.apache.qpid.proton.amqp.transport.ErrorCondition;


public class RejectedType  extends AbstractDescribedType<Rejected,List> implements DescribedTypeConstructor<Rejected>
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000025L), Symbol.valueOf("amqp:rejected:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000025L);

    private RejectedType(EncoderImpl encoder)
    {
        super(encoder);
    }

    public UnsignedLong getDescriptor()
    {
        return DESCRIPTOR;
    }

    @Override
    protected List wrap(Rejected val)
    {
        return new RejectedWrapper(val);
    }


    private static final class RejectedWrapper extends AbstractList
    {
        private final Rejected _impl;

        private RejectedWrapper(Rejected impl)
        {
            _impl = impl;
        }

        public Object get(final int index)
        {

            switch(index)
            {
                case 0:
                    return _impl.getError();
            }

            throw new IllegalStateException("Unknown index " + index);

        }

        public int size()
        {
            return _impl.getError() != null
                      ? 1
                      : 0;

        }
    }

    public Rejected newInstance(Object described)
    {
        List l = (List) described;

        Rejected o = new Rejected();

        switch(1 - l.size())
        {
            case 0:
                o.setError( (ErrorCondition) l.get( 0 ) );
        }


        return o;
    }

    public Class<Rejected> getTypeClass()
    {
        return Rejected.class;
    }


    public static void register(Decoder decoder, EncoderImpl encoder)
    {
        RejectedType type = new RejectedType(encoder);
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, type);
        }
        encoder.register(type);
    }
}
  