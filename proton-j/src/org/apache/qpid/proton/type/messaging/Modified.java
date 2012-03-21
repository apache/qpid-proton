
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
import java.util.Map;
import java.util.List;
import java.util.AbstractList;


import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.type.*;


public class Modified
      implements DescribedType , org.apache.qpid.proton.type.transport.DeliveryState, Outcome
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000027L), Symbol.valueOf("amqp:modified:list"), 
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000027L);
    private final ModifiedWrapper _wrapper = new ModifiedWrapper();
    
    private Boolean _deliveryFailed;
    private Boolean _undeliverableHere;
    private Map _messageAnnotations;

    public Boolean getDeliveryFailed()
    {
        return _deliveryFailed;
    }

    public void setDeliveryFailed(Boolean deliveryFailed)
    {
        _deliveryFailed = deliveryFailed;
    }

    public Boolean getUndeliverableHere()
    {
        return _undeliverableHere;
    }

    public void setUndeliverableHere(Boolean undeliverableHere)
    {
        _undeliverableHere = undeliverableHere;
    }

    public Map getMessageAnnotations()
    {
        return _messageAnnotations;
    }

    public void setMessageAnnotations(Map messageAnnotations)
    {
        _messageAnnotations = messageAnnotations;
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
                return _deliveryFailed;
            case 1:
                return _undeliverableHere;
            case 2:
                return _messageAnnotations;            
        }

        throw new IllegalStateException("Unknown index " + index);

    }

    public int size()
    {
        return _messageAnnotations != null 
                  ? 3 
                  : _undeliverableHere != null 
                  ? 2 
                  : _deliveryFailed != null 
                  ? 1 
                  : 0;        

    }


    public final class ModifiedWrapper extends AbstractList
    {

        @Override
        public Object get(final int index)
        {
            return Modified.this.get(index);
        }

        @Override
        public int size()
        {
            return Modified.this.size();
        }
    }

    private static class ModifiedConstructor implements DescribedTypeConstructor<Modified>
    {
        public Modified newInstance(Object described)
        {
            List l = (List) described;

            Modified o = new Modified();


            switch(3 - l.size())
            {

                case 0:
                    o.setMessageAnnotations( (Map) l.get( 2 ) );
                case 1:
                    o.setUndeliverableHere( (Boolean) l.get( 1 ) );
                case 2:
                    o.setDeliveryFailed( (Boolean) l.get( 0 ) );
            }


            return o;
        }

        public Class<Modified> getTypeClass()
        {
            return Modified.class;
        }
    }


    public static void register(Decoder decoder)
    {
        ModifiedConstructor constructor = new ModifiedConstructor();
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, constructor);
        }
    }
}
  