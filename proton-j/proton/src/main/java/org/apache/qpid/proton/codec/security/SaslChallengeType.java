
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
import org.apache.qpid.proton.amqp.security.SaslChallenge;
import org.apache.qpid.proton.codec.AbstractDescribedType;
import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.codec.EncoderImpl;


public class SaslChallengeType extends AbstractDescribedType<SaslChallenge,List> implements DescribedTypeConstructor<SaslChallenge>
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000042L), Symbol.valueOf("amqp:sasl-challenge:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000042L);

    private SaslChallengeType(EncoderImpl encoder)
    {
        super(encoder);
    }

    public UnsignedLong getDescriptor()
    {
        return DESCRIPTOR;
    }

    @Override
    protected List wrap(SaslChallenge val)
    {
        return Collections.singletonList(val.getChallenge());
    }



    public SaslChallenge newInstance(Object described)
    {
        List l = (List) described;

        SaslChallenge o = new SaslChallenge();

        if(l.isEmpty())
        {
            throw new DecodeException("The challenge field cannot be omitted");
        }

        o.setChallenge( (Binary) l.get( 0 ) );



        return o;
    }

    public Class<SaslChallenge> getTypeClass()
    {
        return SaslChallenge.class;
    }



    public static void register(Decoder decoder, EncoderImpl encoder)
    {
        SaslChallengeType type = new SaslChallengeType(encoder);
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, type);
        }
        encoder.register(type);
    }


}
