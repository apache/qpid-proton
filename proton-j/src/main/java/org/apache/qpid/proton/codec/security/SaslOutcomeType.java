
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


package org.apache.qpid.proton.codec.security;

import java.util.AbstractList;
import java.util.List;
import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.UnsignedByte;
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.security.SaslCode;
import org.apache.qpid.proton.amqp.security.SaslOutcome;
import org.apache.qpid.proton.codec.AbstractDescribedType;
import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.codec.EncoderImpl;


public class SaslOutcomeType  extends AbstractDescribedType<SaslOutcome,List> implements DescribedTypeConstructor<SaslOutcome>
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000044L), Symbol.valueOf("amqp:sasl-outcome:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000044L);

    private SaslOutcomeType(EncoderImpl encoder)
    {
        super(encoder);
    }

    public UnsignedLong getDescriptor()
    {
        return DESCRIPTOR;
    }

    @Override
    protected List wrap(SaslOutcome val)
    {
        return new SaslOutcomeWrapper(val);
    }


    public static final class SaslOutcomeWrapper extends AbstractList
    {
        private final SaslOutcome _impl;

        public SaslOutcomeWrapper(SaslOutcome impl)
        {
            _impl = impl;
        }


        public Object get(final int index)
        {

            switch(index)
            {
                case 0:
                    return _impl.getCode().getValue();
                case 1:
                    return _impl.getAdditionalData();
            }

            throw new IllegalStateException("Unknown index " + index);

        }

        public int size()
        {
            return _impl.getAdditionalData() != null
                      ? 2
                      : 1;
        }
    }

    public SaslOutcome newInstance(Object described)
    {
        List l = (List) described;

        SaslOutcome o = new SaslOutcome();

        if(l.isEmpty())
        {
            throw new DecodeException("The code field cannot be omitted");
        }

        switch(2 - l.size())
        {

            case 0:
                o.setAdditionalData( (Binary) l.get( 1 ) );
            case 1:
                o.setCode(SaslCode.valueOf((UnsignedByte) l.get(0)));
        }


        return o;
    }

    public Class<SaslOutcome> getTypeClass()
    {
        return SaslOutcome.class;
    }



    public static void register(Decoder decoder, EncoderImpl encoder)
    {
        SaslOutcomeType type = new SaslOutcomeType(encoder);
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, type);
        }
        encoder.register(type);
    }
}
