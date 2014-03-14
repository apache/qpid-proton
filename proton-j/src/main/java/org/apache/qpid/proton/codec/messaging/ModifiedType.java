
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
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.messaging.Modified;
import org.apache.qpid.proton.codec.AbstractDescribedType;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.codec.EncoderImpl;


public class ModifiedType  extends AbstractDescribedType<Modified,List> implements DescribedTypeConstructor<Modified>
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000027L), Symbol.valueOf("amqp:modified:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000027L);

    private ModifiedType(EncoderImpl encoder)
    {
        super(encoder);
    }

    public UnsignedLong getDescriptor()
    {
        return DESCRIPTOR;
    }

    @Override
    protected List wrap(Modified val)
    {
        return new ModifiedWrapper(val);
    }


    public final class ModifiedWrapper extends AbstractList
    {
        private final Modified _impl;

        public ModifiedWrapper(Modified impl)
        {
            _impl = impl;
        }

        public Object get(final int index)
        {

            switch(index)
            {
                case 0:
                    return _impl.getDeliveryFailed();
                case 1:
                    return _impl.getUndeliverableHere();
                case 2:
                    return _impl.getMessageAnnotations();
            }

            throw new IllegalStateException("Unknown index " + index);

        }

        public int size()
        {
            return _impl.getMessageAnnotations() != null
                      ? 3
                      : _impl.getUndeliverableHere() != null
                      ? 2
                      : _impl.getDeliveryFailed() != null
                      ? 1
                      : 0;

        }

    }

    public Modified newInstance(Object described)
    {
        List l = (List) described;

        Modified o = new Modified();


        switch(3 - l.size())
        {

            case 0:
                o.setMessageAnnotations( (Map) l.get( 2 ) );
            case 1:
                o.setUndeliverableHere( (Boolean) l.get( 1 ) );
            case 2:
                o.setDeliveryFailed( (Boolean) l.get( 0 ) );
        }


        return o;
    }

    public Class<Modified> getTypeClass()
    {
        return Modified.class;
    }



    public static void register(Decoder decoder, EncoderImpl encoder)
    {
        ModifiedType type = new ModifiedType(encoder);
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, type);
        }
        encoder.register(type);
    }
}
  