
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
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.transaction.Coordinator;
import org.apache.qpid.proton.codec.AbstractDescribedType;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.codec.EncoderImpl;


public class CoordinatorType extends AbstractDescribedType<Coordinator,List> implements DescribedTypeConstructor<Coordinator>
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000030L), Symbol.valueOf("amqp:coordinator:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000030L);

    private CoordinatorType(EncoderImpl encoder)
    {
        super(encoder);
    }

    public UnsignedLong getDescriptor()
    {
        return DESCRIPTOR;
    }

    @Override
    protected List wrap(Coordinator val)
    {
        Symbol[] capabilities = val.getCapabilities();
        return capabilities == null || capabilities.length == 0
                ? Collections.EMPTY_LIST
                : Collections.singletonList(capabilities);
    }


    public Coordinator newInstance(Object described)
    {
        List l = (List) described;

        Coordinator o = new Coordinator();


        switch(1 - l.size())
        {

            case 0:
                Object val0 = l.get( 0 );
                if( val0 == null || val0.getClass().isArray() )
                {
                    o.setCapabilities( (Symbol[]) val0 );
                }
                else
                {
                    o.setCapabilities( (Symbol) val0 );
                }
        }


        return o;
    }

    public Class<Coordinator> getTypeClass()
    {
        return Coordinator.class;
    }



    public static void register(Decoder decoder, EncoderImpl encoder)
    {
        CoordinatorType type = new CoordinatorType(encoder);
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, type);
        }
        encoder.register(type);
    }
}
  