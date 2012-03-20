
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


public class DeleteOnNoMessages
      implements DescribedType , LifetimePolicy
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x000000000000002dL), Symbol.valueOf("amqp:delete-on-no-messages:list"), 
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x000000000000002dL);
    private final DeleteOnNoMessagesWrapper _wrapper = new DeleteOnNoMessagesWrapper();
    
    
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


    public final class DeleteOnNoMessagesWrapper extends AbstractList
    {

        @Override
        public Object get(final int index)
        {
            return DeleteOnNoMessages.this.get(index);
        }

        @Override
        public int size()
        {
            return DeleteOnNoMessages.this.size();
        }
    }

    private static class DeleteOnNoMessagesConstructor implements DescribedTypeConstructor<DeleteOnNoMessages>
    {
        public DeleteOnNoMessages newInstance(Object described)
        {
            List l = (List) described;

            DeleteOnNoMessages o = new DeleteOnNoMessages();


            return o;
        }

        public Class<DeleteOnNoMessages> getTypeClass()
        {
            return DeleteOnNoMessages.class;
        }
    }


    public static void register(Decoder decoder)
    {
        DeleteOnNoMessagesConstructor constructor = new DeleteOnNoMessagesConstructor();
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, constructor);
        }
    }
}
  