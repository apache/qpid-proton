
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

import java.util.Collections;
import java.util.List;
import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.security.SaslResponse;
import org.apache.qpid.proton.codec.AbstractDescribedType;
import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.codec.EncoderImpl;
import org.apache.qpid.proton.codec.ReadableBuffer;
import org.apache.qpid.proton.codec.TypeConstructor;


public class SaslResponseType extends AbstractDescribedType<SaslResponse,List> implements DescribedTypeConstructor<SaslResponse>
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000043L), Symbol.valueOf("amqp:sasl-response:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000043L);

    private SaslResponseType(EncoderImpl encoder)
    {
        super(encoder);
    }

    public UnsignedLong getDescriptor()
    {
        return DESCRIPTOR;
    }

    @Override
    protected List wrap(SaslResponse val)
    {
        return Collections.singletonList(val.getResponse());
    }


    public SaslResponse newInstance(ReadableBuffer buffer, TypeConstructor constructor)
    {
        List l = (List) constructor.readValue(buffer);

        SaslResponse o = new SaslResponse();

        if(l.isEmpty())
        {
            throw new DecodeException("The response field cannot be omitted");
        }

        switch(1 - l.size())
        {

            case 0:
                o.setResponse( (Binary) l.get( 0 ) );
        }


        return o;
    }

    public Class<SaslResponse> getTypeClass()
    {
        return SaslResponse.class;
    }



    public static void register(Decoder decoder, EncoderImpl encoder)
    {
        SaslResponseType type = new SaslResponseType(encoder);
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, type);
        }
        encoder.register(type);
    }
}
