
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


public class DeleteOnNoLinks
      implements DescribedType , LifetimePolicy
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x000000000000002cL), Symbol.valueOf("amqp:delete-on-no-links:list"), 
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x000000000000002cL);
    private final DeleteOnNoLinksWrapper _wrapper = new DeleteOnNoLinksWrapper();
    
    
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


    public final class DeleteOnNoLinksWrapper extends AbstractList
    {

        @Override
        public Object get(final int index)
        {
            return DeleteOnNoLinks.this.get(index);
        }

        @Override
        public int size()
        {
            return DeleteOnNoLinks.this.size();
        }
    }

    private static class DeleteOnNoLinksConstructor implements DescribedTypeConstructor<DeleteOnNoLinks>
    {
        public DeleteOnNoLinks newInstance(Object described)
        {
            List l = (List) described;

            DeleteOnNoLinks o = new DeleteOnNoLinks();


            return o;
        }

        public Class<DeleteOnNoLinks> getTypeClass()
        {
            return DeleteOnNoLinks.class;
        }
    }


    public static void register(Decoder decoder)
    {
        DeleteOnNoLinksConstructor constructor = new DeleteOnNoLinksConstructor();
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, constructor);
        }
    }
}
  