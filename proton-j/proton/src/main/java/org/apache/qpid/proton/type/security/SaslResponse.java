
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


public class SaslResponse
      implements DescribedType , SaslFrameBody
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000043L), Symbol.valueOf("amqp:sasl-response:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000043L);
    private final SaslResponseWrapper _wrapper = new SaslResponseWrapper();

    private Binary _response;

    public Binary getResponse()
    {
        return _response;
    }

    public void setResponse(Binary response)
    {
        if( response == null )
        {
            throw new NullPointerException("the response field is mandatory");
        }

        _response = response;
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
                return _response;
        }

        throw new IllegalStateException("Unknown index " + index);

    }

    public int size()
    {
        return 1;

    }

    @Override
    public String toString()
    {
        return "SaslResponse{response=" + _response +
               '}';
    }

    public <E> void invoke(SaslFrameBodyHandler<E> handler, Binary payload, E context)
    {
        handler.handleResponse(this, payload, context);
    }


    public final class SaslResponseWrapper extends AbstractList
    {

        @Override
        public Object get(final int index)
        {
            return SaslResponse.this.get(index);
        }

        @Override
        public int size()
        {
            return SaslResponse.this.size();
        }
    }

    private static class SaslResponseConstructor implements DescribedTypeConstructor<SaslResponse>
    {
        public SaslResponse newInstance(Object described)
        {
            List l = (List) described;

            SaslResponse o = new SaslResponse();

            if(l.size() <= 0)
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
    }


    public static void register(Decoder decoder)
    {
        SaslResponseConstructor constructor = new SaslResponseConstructor();
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, constructor);
        }
    }
}
