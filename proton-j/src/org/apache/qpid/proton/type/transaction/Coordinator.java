
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


package org.apache.qpid.proton.type.transaction;
import java.util.List;
import java.util.AbstractList;


import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.type.*;


public class Coordinator
      implements DescribedType , org.apache.qpid.proton.type.transport.Target
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000030L), Symbol.valueOf("amqp:coordinator:list"), 
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000030L);
    private final CoordinatorWrapper _wrapper = new CoordinatorWrapper();
    
    private Symbol[] _capabilities;

    public Symbol[] getCapabilities()
    {
        return _capabilities;
    }

    public void setCapabilities(Symbol... capabilities)
    {
        _capabilities = capabilities;
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
                return _capabilities;            
        }

        throw new IllegalStateException("Unknown index " + index);

    }

    public int size()
    {
        return _capabilities != null 
                  ? 1 
                  : 0;        

    }


    public final class CoordinatorWrapper extends AbstractList
    {

        @Override
        public Object get(final int index)
        {
            return Coordinator.this.get(index);
        }

        @Override
        public int size()
        {
            return Coordinator.this.size();
        }
    }

    private static class CoordinatorConstructor implements DescribedTypeConstructor<Coordinator>
    {
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
    }


    public static void register(Decoder decoder)
    {
        CoordinatorConstructor constructor = new CoordinatorConstructor();
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, constructor);
        }
    }
}
  