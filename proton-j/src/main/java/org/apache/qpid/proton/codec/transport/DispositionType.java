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
import org.apache.qpid.proton.codec.DecoderImpl;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.codec.EncoderImpl;
import org.apache.qpid.proton.codec.ListType;
import org.apache.qpid.proton.codec.ReadableBuffer;
import org.apache.qpid.proton.codec.TypeConstructor;


public final class DispositionType extends AbstractDescribedType<Disposition, List> implements DescribedTypeConstructor<Disposition>
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000015L), Symbol.valueOf("amqp:disposition:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000015L);

    private final DecoderImpl _decoder;

    private DispositionType(DecoderImpl decoder, EncoderImpl encoder)
    {
        super(encoder);
        this._decoder = decoder;
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

    @Override
    public Disposition newInstance(ReadableBuffer buffer, TypeConstructor constructor)
    {

        ListType.ListEncoding listEncoding = (ListType.ListEncoding)constructor;

        // This is for the prototype only, we should use the ListType directly here
        int size = listEncoding.readCount(buffer, _decoder);

        if (size == 0)
        {
            throw new DecodeException("The first field cannot be omitted");
        }

        Disposition o = new Disposition();

        for (int i = 0 ; i < size; i++)
        {
            Object elementValue = listEncoding.readElement(buffer, _decoder);
            switch (i)
            {
                case 0:
                    o.setRole(Boolean.TRUE.equals(elementValue) ? Role.RECEIVER : Role.SENDER);
                    break;
                case 1:
                    o.setFirst((UnsignedInteger) elementValue);
                    break;
                case 2:
                    o.setLast((UnsignedInteger) elementValue);
                    break;
                case 3:
                    Boolean settled = (Boolean) elementValue;
                    o.setSettled(settled == null ? false : settled);
                    break;
                case 4:
                    o.setState((DeliveryState) elementValue);
                    break;
                case 5:
                    Boolean batchable = (Boolean) elementValue;
                    o.setBatchable(batchable == null ? false : batchable);
                    break;
            }

        }

        return o;
    }

    public Class<Disposition> getTypeClass()
    {
        return Disposition.class;
    }


    public static void register(DecoderImpl decoder, EncoderImpl encoder)
    {
        DispositionType type = new DispositionType(decoder, encoder);
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, type);
        }
        encoder.register(type);
    }


}
