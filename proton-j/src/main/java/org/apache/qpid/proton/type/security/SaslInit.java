
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


public class SaslInit
      implements DescribedType , SaslFrameBody
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000041L), Symbol.valueOf("amqp:sasl-init:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000041L);
    private final SaslInitWrapper _wrapper = new SaslInitWrapper();

    private Symbol _mechanism;
    private Binary _initialResponse;
    private String _hostname;

    public Symbol getMechanism()
    {
        return _mechanism;
    }

    public void setMechanism(Symbol mechanism)
    {
        if( mechanism == null )
        {
            throw new NullPointerException("the mechanism field is mandatory");
        }

        _mechanism = mechanism;
    }

    public Binary getInitialResponse()
    {
        return _initialResponse;
    }

    public void setInitialResponse(Binary initialResponse)
    {
        _initialResponse = initialResponse;
    }

    public String getHostname()
    {
        return _hostname;
    }

    public void setHostname(String hostname)
    {
        _hostname = hostname;
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
                return _mechanism;
            case 1:
                return _initialResponse;
            case 2:
                return _hostname;
        }

        throw new IllegalStateException("Unknown index " + index);

    }

    public int size()
    {
        return _hostname != null
                  ? 3
                  : _initialResponse != null
                  ? 2
                  : 1;

    }

    @Override
    public String toString()
    {
        return "SaslInit{" +
               "mechanism=" + _mechanism +
               ", initialResponse=" + _initialResponse +
               ", hostname=" + (_hostname == null ? "null":'\''+ _hostname + '\'') +
               '}';
    }

    public <E> void invoke(SaslFrameBodyHandler<E> handler, Binary payload, E context)
    {
        handler.handleInit(this, payload, context);
    }


    public final class SaslInitWrapper extends AbstractList
    {

        @Override
        public Object get(final int index)
        {
            return SaslInit.this.get(index);
        }

        @Override
        public int size()
        {
            return SaslInit.this.size();
        }
    }

    private static class SaslInitConstructor implements DescribedTypeConstructor<SaslInit>
    {
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
    }


    public static void register(Decoder decoder)
    {
        SaslInitConstructor constructor = new SaslInitConstructor();
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, constructor);
        }
    }
}
