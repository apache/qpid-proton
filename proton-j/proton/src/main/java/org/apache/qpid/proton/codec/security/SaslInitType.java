
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
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.security.SaslInit;
import org.apache.qpid.proton.codec.AbstractDescribedType;
import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.codec.EncoderImpl;


public class SaslInitType extends AbstractDescribedType<SaslInit,List> implements DescribedTypeConstructor<SaslInit>
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000041L), Symbol.valueOf("amqp:sasl-init:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000041L);

    private SaslInitType(EncoderImpl encoder)
    {
        super(encoder);
    }

    public UnsignedLong getDescriptor()
    {
        return DESCRIPTOR;
    }

    @Override
    protected List wrap(SaslInit val)
    {
        return new SaslInitWrapper(val);
    }


    public static class SaslInitWrapper extends AbstractList
    {

        private SaslInit _saslInit;

        public SaslInitWrapper(SaslInit saslInit)
        {
            _saslInit = saslInit;
        }

        public Object get(final int index)
        {

            switch(index)
            {
                case 0:
                    return _saslInit.getMechanism();
                case 1:
                    return _saslInit.getInitialResponse();
                case 2:
                    return _saslInit.getHostname();
            }

            throw new IllegalStateException("Unknown index " + index);

        }

        public int size()
        {
            return _saslInit.getHostname() != null
                      ? 3
                      : _saslInit.getInitialResponse() != null
                      ? 2
                      : 1;

        }
    }

    public SaslInit newInstance(Object described)
    {
        List l = (List) described;

        SaslInit o = new SaslInit();

        if(l.size() <= 0)
        {
            throw new DecodeException("The mechanism field cannot be omitted");
        }

        switch(3 - l.size())
        {

            case 0:
                o.setHostname( (String) l.get( 2 ) );
            case 1:
                o.setInitialResponse( (Binary) l.get( 1 ) );
            case 2:
                o.setMechanism( (Symbol) l.get( 0 ) );
        }


        return o;
    }

    public Class<SaslInit> getTypeClass()
    {
        return SaslInit.class;
    }



    public static void register(Decoder decoder, EncoderImpl encoder)
    {
        SaslInitType type = new SaslInitType(encoder);
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, type);
        }
        encoder.register(type);
    }
}
