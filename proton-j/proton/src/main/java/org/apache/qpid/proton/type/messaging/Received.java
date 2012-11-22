
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


public class Received
      implements DescribedType , org.apache.qpid.proton.type.transport.DeliveryState
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000023L), Symbol.valueOf("amqp:received:list"), 
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000023L);
    private final ReceivedWrapper _wrapper = new ReceivedWrapper();
    
    private UnsignedInteger _sectionNumber;
    private UnsignedLong _sectionOffset;

    public UnsignedInteger getSectionNumber()
    {
        return _sectionNumber;
    }

    public void setSectionNumber(UnsignedInteger sectionNumber)
    {
        _sectionNumber = sectionNumber;
    }

    public UnsignedLong getSectionOffset()
    {
        return _sectionOffset;
    }

    public void setSectionOffset(UnsignedLong sectionOffset)
    {
        _sectionOffset = sectionOffset;
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
                return _sectionNumber;
            case 1:
                return _sectionOffset;            
        }

        throw new IllegalStateException("Unknown index " + index);

    }

    public int size()
    {
        return _sectionOffset != null 
                  ? 2 
                  : _sectionNumber != null 
                  ? 1 
                  : 0;        

    }


    public final class ReceivedWrapper extends AbstractList
    {

        @Override
        public Object get(final int index)
        {
            return Received.this.get(index);
        }

        @Override
        public int size()
        {
            return Received.this.size();
        }
    }

    private static class ReceivedConstructor implements DescribedTypeConstructor<Received>
    {
        public Received newInstance(Object described)
        {
            List l = (List) described;

            Received o = new Received();


            switch(2 - l.size())
            {

                case 0:
                    o.setSectionOffset( (UnsignedLong) l.get( 1 ) );
                case 1:
                    o.setSectionNumber( (UnsignedInteger) l.get( 0 ) );
            }


            return o;
        }

        public Class<Received> getTypeClass()
        {
            return Received.class;
        }
    }


    public static void register(Decoder decoder)
    {
        ReceivedConstructor constructor = new ReceivedConstructor();
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, constructor);
        }
    }
}
  