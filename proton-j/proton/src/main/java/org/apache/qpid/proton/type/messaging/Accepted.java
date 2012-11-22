
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


package org.apache.qpid.proton.type.messaging;
import java.util.List;
import java.util.AbstractList;


import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.type.*;


public class Accepted
      implements DescribedType , org.apache.qpid.proton.type.transport.DeliveryState, Outcome
{
    private static final Accepted INSTANCE = new Accepted();

    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000024L), Symbol.valueOf("amqp:accepted:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000024L);
    private final AcceptedWrapper _wrapper = new AcceptedWrapper();


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

        throw new IllegalStateException("Unknown index " + index);

    }

    public int size()
    {
        return 0;

    }


    public final class AcceptedWrapper extends AbstractList
    {

        @Override
        public Object get(final int index)
        {
            return Accepted.this.get(index);
        }

        @Override
        public int size()
        {
            return Accepted.this.size();
        }
    }

    private static class AcceptedConstructor implements DescribedTypeConstructor<Accepted>
    {

        public Accepted newInstance(Object described)
        {
            List l = (List) described;

            Accepted o = new Accepted();


            return INSTANCE;
        }

        public Class<Accepted> getTypeClass()
        {
            return Accepted.class;
        }
    }


    public static void register(Decoder decoder)
    {
        AcceptedConstructor constructor = new AcceptedConstructor();
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, constructor);
        }
    }

    @Override
    public String toString()
    {
        return "Accepted{}";
    }

    public static Accepted getInstance()
    {
        return INSTANCE;
    }
}
