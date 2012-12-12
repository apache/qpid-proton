
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
import org.apache.qpid.proton.amqp.transport.DeliveryState;
import org.apache.qpid.proton.amqp.transport.Disposition;
import org.apache.qpid.proton.amqp.transport.Role;
import org.apache.qpid.proton.codec.AbstractDescribedType;
import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.codec.EncoderImpl;


public final class DispositionType extends AbstractDescribedType<Disposition,List> implements DescribedTypeConstructor<Disposition>
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000015L), Symbol.valueOf("amqp:disposition:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000015L);

    private DispositionType(EncoderImpl encoder)
    {
        super(encoder);
    }

    public UnsignedLong getDescriptor()
    {
        return DESCRIPTOR;
    }

    @Override
    protected List wrap(Disposition val)
    {
        return new DispositionWrapper(val);
    }


    private static final class DispositionWrapper extends AbstractList
    {

        private Disposition _disposition;

        public DispositionWrapper(Disposition disposition)
        {
            _disposition = disposition;
        }

        public Object get(final int index)
        {

            switch(index)
            {
                case 0:
                    return _disposition.getRole().getValue();
                case 1:
                    return _disposition.getFirst();
                case 2:
                    return _disposition.getLast();
                case 3:
                    return _disposition.getSettled();
                case 4:
                    return _disposition.getState();
                case 5:
                    return _disposition.getBatchable();
            }

            throw new IllegalStateException("Unknown index " + index);

        }

        public int size()
        {
            return _disposition.getBatchable()
                      ? 6
                      : _disposition.getState() != null
                      ? 5
                      : _disposition.getSettled()
                      ? 4
                      : _disposition.getLast() != null
                      ? 3
                      : 2;

        }
    }

        public Disposition newInstance(Object described)
        {
            List l = (List) described;

            Disposition o = new Disposition();

            if(l.isEmpty())
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
                    o.setRole( Boolean.TRUE.equals(l.get( 0 )) ? Role.RECEIVER : Role.SENDER );
            }


            return o;
        }

        public Class<Disposition> getTypeClass()
        {
            return Disposition.class;
        }




    public static void register(Decoder decoder, EncoderImpl encoder)
    {
        DispositionType type = new DispositionType(encoder);
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, type);
        }
        encoder.register(type);
    }


}
