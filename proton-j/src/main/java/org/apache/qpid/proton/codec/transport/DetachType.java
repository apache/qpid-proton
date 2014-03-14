
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


package org.apache.qpid.proton.codec.transport;

import java.util.AbstractList;
import java.util.List;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.UnsignedInteger;
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.transport.Detach;
import org.apache.qpid.proton.amqp.transport.ErrorCondition;
import org.apache.qpid.proton.codec.AbstractDescribedType;
import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.codec.EncoderImpl;


public final class DetachType extends AbstractDescribedType<Detach,List> implements DescribedTypeConstructor<Detach>
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000016L), Symbol.valueOf("amqp:detach:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000016L);

    private DetachType(EncoderImpl encoder)
    {
        super(encoder);
    }

    public UnsignedLong getDescriptor()
    {
        return DESCRIPTOR;
    }

    @Override
    protected List wrap(Detach val)
    {
        return new DetachWrapper(val);
    }

    public static class DetachWrapper extends AbstractList
    {

        private Detach _detach;

        public DetachWrapper(Detach detach)
        {
            _detach = detach;
        }

        public Object get(final int index)
        {

            switch(index)
            {
                case 0:
                    return _detach.getHandle();
                case 1:
                    return _detach.getClosed();
                case 2:
                    return _detach.getError();
            }

            throw new IllegalStateException("Unknown index " + index);

        }

        public int size()
        {
            return _detach.getError() != null
                      ? 3
                      : _detach.getClosed()
                      ? 2
                      : 1;

        }
    }

    public Detach newInstance(Object described)
    {
        List l = (List) described;

        Detach o = new Detach();

        if(l.isEmpty())
        {
            throw new DecodeException("The handle field cannot be omitted");
        }

        switch(3 - l.size())
        {

            case 0:
                o.setError( (ErrorCondition) l.get( 2 ) );
            case 1:
                Boolean closed = (Boolean) l.get(1);
                o.setClosed(closed == null ? false : closed);
            case 2:
                o.setHandle( (UnsignedInteger) l.get( 0 ) );
        }


        return o;
    }

    public Class<Detach> getTypeClass()
    {
        return Detach.class;
    }


    public static void register(Decoder decoder, EncoderImpl encoder)
    {
        DetachType type = new DetachType(encoder);
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, type);
        }
        encoder.register(type);
    }

}
  