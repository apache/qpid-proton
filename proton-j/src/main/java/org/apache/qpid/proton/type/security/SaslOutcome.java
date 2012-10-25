
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


public class SaslOutcome
      implements DescribedType , SaslFrameBody
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000044L), Symbol.valueOf("amqp:sasl-outcome:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000044L);
    private final SaslOutcomeWrapper _wrapper = new SaslOutcomeWrapper();

    private UnsignedByte _code;
    private Binary _additionalData;

    public UnsignedByte getCode()
    {
        return _code;
    }

    public void setCode(UnsignedByte code)
    {
        if( code == null )
        {
            throw new NullPointerException("the code field is mandatory");
        }

        _code = code;
    }

    public Binary getAdditionalData()
    {
        return _additionalData;
    }

    public void setAdditionalData(Binary additionalData)
    {
        _additionalData = additionalData;
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
                return _code;
            case 1:
                return _additionalData;
        }

        throw new IllegalStateException("Unknown index " + index);

    }

    public int size()
    {
        return _additionalData != null
                  ? 2
                  : 1;

    }

    @Override
    public String toString()
    {
        return "SaslOutcome{" +
               "_code=" + _code +
               ", _additionalData=" + _additionalData +
               '}';
    }

    public <E> void invoke(SaslFrameBodyHandler<E> handler, Binary payload, E context)
    {
        handler.handleOutcome(this, payload, context);
    }


    public final class SaslOutcomeWrapper extends AbstractList
    {

        @Override
        public Object get(final int index)
        {
            return SaslOutcome.this.get(index);
        }

        @Override
        public int size()
        {
            return SaslOutcome.this.size();
        }
    }

    private static class SaslOutcomeConstructor implements DescribedTypeConstructor<SaslOutcome>
    {
        public SaslOutcome newInstance(Object described)
        {
            List l = (List) described;

            SaslOutcome o = new SaslOutcome();

            if(l.size() <= 0)
            {
                throw new DecodeException("The code field cannot be omitted");
            }

            switch(2 - l.size())
            {

                case 0:
                    o.setAdditionalData( (Binary) l.get( 1 ) );
                case 1:
                    o.setCode( (UnsignedByte) l.get( 0 ) );
            }


            return o;
        }

        public Class<SaslOutcome> getTypeClass()
        {
            return SaslOutcome.class;
        }
    }


    public static void register(Decoder decoder)
    {
        SaslOutcomeConstructor constructor = new SaslOutcomeConstructor();
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, constructor);
        }
    }
}
