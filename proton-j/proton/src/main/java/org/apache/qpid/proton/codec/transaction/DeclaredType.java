
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


package org.apache.qpid.proton.codec.transaction;

import java.util.Collections;
import java.util.List;
import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.transaction.Declared;
import org.apache.qpid.proton.codec.AbstractDescribedType;
import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.codec.EncoderImpl;


public class DeclaredType extends AbstractDescribedType<Declared,List> implements DescribedTypeConstructor<Declared>
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000033L), Symbol.valueOf("amqp:declared:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000033L);

    private DeclaredType(EncoderImpl encoder)
    {
        super(encoder);
    }

    public UnsignedLong getDescriptor()
    {
        return DESCRIPTOR;
    }

    @Override
    protected List wrap(Declared val)
    {
        return Collections.singletonList(val.getTxnId());
    }

    public Declared newInstance(Object described)
    {
        List l = (List) described;

        Declared o = new Declared();

        if(l.isEmpty())
        {
            throw new DecodeException("The txn-id field cannot be omitted");
        }

        o.setTxnId( (Binary) l.get( 0 ) );



        return o;
    }

    public Class<Declared> getTypeClass()
    {
        return Declared.class;
    }



    public static void register(Decoder decoder, EncoderImpl encoder)
    {
        DeclaredType type = new DeclaredType(encoder);
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, type);
        }
        encoder.register(type);
    }
}
  