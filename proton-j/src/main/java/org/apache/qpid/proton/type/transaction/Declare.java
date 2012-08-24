
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


public class Declare
      implements DescribedType 
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000031L), Symbol.valueOf("amqp:declare:list"), 
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000031L);
    private final DeclareWrapper _wrapper = new DeclareWrapper();
    
    private GlobalTxId _globalId;

    public GlobalTxId getGlobalId()
    {
        return _globalId;
    }

    public void setGlobalId(GlobalTxId globalId)
    {
        _globalId = globalId;
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
                return _globalId;            
        }

        throw new IllegalStateException("Unknown index " + index);

    }

    public int size()
    {
        return _globalId != null 
                  ? 1 
                  : 0;        

    }


    public final class DeclareWrapper extends AbstractList
    {

        @Override
        public Object get(final int index)
        {
            return Declare.this.get(index);
        }

        @Override
        public int size()
        {
            return Declare.this.size();
        }
    }

    private static class DeclareConstructor implements DescribedTypeConstructor<Declare>
    {
        public Declare newInstance(Object described)
        {
            List l = (List) described;

            Declare o = new Declare();


            switch(1 - l.size())
            {

                case 0:
                    o.setGlobalId( (GlobalTxId) l.get( 0 ) );
            }


            return o;
        }

        public Class<Declare> getTypeClass()
        {
            return Declare.class;
        }
    }


    public static void register(Decoder decoder)
    {
        DeclareConstructor constructor = new DeclareConstructor();
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, constructor);
        }
    }
}
  