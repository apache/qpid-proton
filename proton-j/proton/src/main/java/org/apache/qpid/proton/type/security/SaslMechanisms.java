
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

import java.util.AbstractList;
import java.util.Arrays;
import java.util.List;
import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.type.Binary;
import org.apache.qpid.proton.type.DescribedType;
import org.apache.qpid.proton.type.Symbol;
import org.apache.qpid.proton.type.UnsignedLong;


public class SaslMechanisms
      implements DescribedType , SaslFrameBody
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000040L), Symbol.valueOf("amqp:sasl-mechanisms:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000040L);
    private final SaslMechanismsWrapper _wrapper = new SaslMechanismsWrapper();

    private Symbol[] _saslServerMechanisms;

    public Symbol[] getSaslServerMechanisms()
    {
        return _saslServerMechanisms;
    }

    public void setSaslServerMechanisms(Symbol... saslServerMechanisms)
    {
        if( saslServerMechanisms == null )
        {
            throw new NullPointerException("the sasl-server-mechanisms field is mandatory");
        }

        _saslServerMechanisms = saslServerMechanisms;
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
                return _saslServerMechanisms;
        }

        throw new IllegalStateException("Unknown index " + index);

    }

    public int size()
    {
        return 1;

    }

    public <E> void invoke(SaslFrameBodyHandler<E> handler, Binary payload, E context)
    {
        handler.handleMechanisms(this, payload, context);
    }


    public final class SaslMechanismsWrapper extends AbstractList
    {

        @Override
        public Object get(final int index)
        {
            return SaslMechanisms.this.get(index);
        }

        @Override
        public int size()
        {
            return SaslMechanisms.this.size();
        }
    }

    private static class SaslMechanismsConstructor implements DescribedTypeConstructor<SaslMechanisms>
    {
        public SaslMechanisms newInstance(Object described)
        {
            List l = (List) described;

            SaslMechanisms o = new SaslMechanisms();

            if(l.size() <= 0)
            {
                throw new DecodeException("The sasl-server-mechanisms field cannot be omitted");
            }

            switch(1 - l.size())
            {

                case 0:
                    Object val0 = l.get( 0 );
                    if( val0 == null || val0.getClass().isArray() )
                    {
                        o.setSaslServerMechanisms( (Symbol[]) val0 );
                    }
                    else
                    {
                        o.setSaslServerMechanisms( (Symbol) val0 );
                    }
            }


            return o;
        }

        public Class<SaslMechanisms> getTypeClass()
        {
            return SaslMechanisms.class;
        }
    }


    public static void register(Decoder decoder)
    {
        SaslMechanismsConstructor constructor = new SaslMechanismsConstructor();
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, constructor);
        }
    }

    @Override
    public String toString()
    {
        return "SaslMechanisms{" +
               "saslServerMechanisms=" + (_saslServerMechanisms == null ? null : Arrays.asList(_saslServerMechanisms))
               +
               '}';
    }
}
