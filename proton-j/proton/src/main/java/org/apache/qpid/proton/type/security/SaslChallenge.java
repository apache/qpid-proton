
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


package org.apache.qpid.proton.type.security;
import java.util.List;
import java.util.AbstractList;


import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.type.*;


public class SaslChallenge
      implements DescribedType , SaslFrameBody
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000042L), Symbol.valueOf("amqp:sasl-challenge:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000042L);
    private final SaslChallengeWrapper _wrapper = new SaslChallengeWrapper();

    private Binary _challenge;

    public Binary getChallenge()
    {
        return _challenge;
    }

    public void setChallenge(Binary challenge)
    {
        if( challenge == null )
        {
            throw new NullPointerException("the challenge field is mandatory");
        }

        _challenge = challenge;
    }

    public Object getDescriptor()
    {
        return DESCRIPTOR;
    }

    public Object getDescribed()
    {
        return _wrapper;
    }

    public Object get(final int index)
    {

        switch(index)
        {
            case 0:
                return _challenge;
        }

        throw new IllegalStateException("Unknown index " + index);

    }

    public int size()
    {
        return 1;

    }

    public <E> void invoke(SaslFrameBodyHandler<E> handler, Binary payload, E context)
    {
        handler.handleChallenge(this, payload, context);
    }


    public final class SaslChallengeWrapper extends AbstractList
    {

        @Override
        public Object get(final int index)
        {
            return SaslChallenge.this.get(index);
        }

        @Override
        public int size()
        {
            return SaslChallenge.this.size();
        }
    }

    private static class SaslChallengeConstructor implements DescribedTypeConstructor<SaslChallenge>
    {
        public SaslChallenge newInstance(Object described)
        {
            List l = (List) described;

            SaslChallenge o = new SaslChallenge();

            if(l.size() <= 0)
            {
                throw new DecodeException("The challenge field cannot be omitted");
            }

            switch(1 - l.size())
            {

                case 0:
                    o.setChallenge( (Binary) l.get( 0 ) );
            }


            return o;
        }

        public Class<SaslChallenge> getTypeClass()
        {
            return SaslChallenge.class;
        }
    }


    public static void register(Decoder decoder)
    {
        SaslChallengeConstructor constructor = new SaslChallengeConstructor();
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, constructor);
        }
    }

    @Override
    public String toString()
    {
        return "SaslChallenge{" +
               "challenge=" + _challenge +
               '}';
    }
}
