
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
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.security.SaslMechanisms;
import org.apache.qpid.proton.codec.AbstractDescribedType;
import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.codec.EncoderImpl;


public class SaslMechanismsType extends AbstractDescribedType<SaslMechanisms,List> implements DescribedTypeConstructor<SaslMechanisms>
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000040L), Symbol.valueOf("amqp:sasl-mechanisms:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000040L);

    private SaslMechanismsType(EncoderImpl encoder)
    {
        super(encoder);
    }

    public UnsignedLong getDescriptor()
    {
        return DESCRIPTOR;
    }

    @Override
    protected List wrap(SaslMechanisms val)
    {
        return Collections.singletonList(val.getSaslServerMechanisms());
    }

    public SaslMechanisms newInstance(Object described)
    {
        List l = (List) described;

        SaslMechanisms o = new SaslMechanisms();

        if(l.isEmpty())
        {
            throw new DecodeException("The sasl-server-mechanisms field cannot be omitted");
        }

        Object val0 = l.get( 0 );
        if( val0 == null || val0.getClass().isArray() )
        {
            o.setSaslServerMechanisms( (Symbol[]) val0 );
        }
        else
        {
            o.setSaslServerMechanisms( (Symbol) val0 );
        }

        return o;
    }

    public Class<SaslMechanisms> getTypeClass()
    {
        return SaslMechanisms.class;
    }



    public static void register(Decoder decoder, EncoderImpl encoder)
    {
        SaslMechanismsType type = new SaslMechanismsType(encoder);
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, type);
        }
        encoder.register(type);
    }


}
