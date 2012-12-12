
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
import java.util.Map;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.UnsignedInteger;
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.messaging.Target;
import org.apache.qpid.proton.codec.AbstractDescribedType;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.codec.EncoderImpl;
import org.apache.qpid.proton.amqp.messaging.TerminusDurability;
import org.apache.qpid.proton.amqp.messaging.TerminusExpiryPolicy;


public class TargetType extends AbstractDescribedType<Target,List> implements DescribedTypeConstructor<Target>
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000029L), Symbol.valueOf("amqp:target:list"), 
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000029L);

    public TargetType(EncoderImpl encoder)
    {
        super(encoder);
    }

    public UnsignedLong getDescriptor()
    {
        return DESCRIPTOR;
    }

    @Override
    protected List wrap(Target val)
    {
        return new TargetWrapper(val);
    }


    private static final class TargetWrapper extends AbstractList
    {
        private final Target _impl;

        public TargetWrapper(Target impl)
        {
            _impl = impl;
        }

        public Object get(final int index)
        {

            switch(index)
            {
                case 0:
                    return _impl.getAddress();
                case 1:
                    return _impl.getDurable().getValue();
                case 2:
                    return _impl.getExpiryPolicy().getPolicy();
                case 3:
                    return _impl.getTimeout();
                case 4:
                    return _impl.getDynamic();
                case 5:
                    return _impl.getDynamicNodeProperties();
                case 6:
                    return _impl.getCapabilities();
            }

            throw new IllegalStateException("Unknown index " + index);

        }

        public int size()
        {
            return _impl.getCapabilities() != null
                      ? 7
                      : _impl.getDynamicNodeProperties() != null
                      ? 6
                      : _impl.getDynamic()
                      ? 5
                      : (_impl.getTimeout() != null && !_impl.getTimeout().equals(UnsignedInteger.ZERO))
                      ? 4
                      : !_impl.getExpiryPolicy().equals(TerminusExpiryPolicy.SESSION_END)
                      ? 3
                      : !_impl.getDurable().equals(TerminusDurability.NONE)
                      ? 2
                      : _impl.getAddress() != null
                      ? 1
                      : 0;

        }
    }

    public Target newInstance(Object described)
    {
        List l = (List) described;

        Target o = new Target();


        switch(7 - l.size())
        {

            case 0:
                Object val0 = l.get( 6 );
                if( val0 == null || val0.getClass().isArray() )
                {
                    o.setCapabilities( (Symbol[]) val0 );
                }
                else
                {
                    o.setCapabilities( (Symbol) val0 );
                }
            case 1:
                o.setDynamicNodeProperties( (Map) l.get( 5 ) );
            case 2:
                Boolean dynamic = (Boolean) l.get(4);
                o.setDynamic(dynamic == null ? false : dynamic);
            case 3:
                UnsignedInteger timeout = (UnsignedInteger) l.get(3);
                o.setTimeout(timeout == null ? UnsignedInteger.ZERO : timeout);
            case 4:
                Symbol expiryPolicy = (Symbol) l.get(2);
                o.setExpiryPolicy(expiryPolicy == null ? TerminusExpiryPolicy.SESSION_END : TerminusExpiryPolicy.valueOf(expiryPolicy));
            case 5:
                UnsignedInteger durable = (UnsignedInteger) l.get(1);
                o.setDurable(durable == null ? TerminusDurability.NONE : TerminusDurability.get(durable));
            case 6:
                o.setAddress( (String) l.get( 0 ) );
        }


        return o;
    }

    public Class<Target> getTypeClass()
    {
        return Target.class;
    }



    public static void register(Decoder decoder, EncoderImpl encoder)
    {
        TargetType type = new TargetType(encoder);
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, type);
        }
        encoder.register(type);
    }

}
  