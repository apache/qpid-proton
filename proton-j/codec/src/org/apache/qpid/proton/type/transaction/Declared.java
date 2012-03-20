
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


import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.type.*;


public class Declared
      implements DescribedType , org.apache.qpid.proton.type.transport.DeliveryState, org.apache.qpid.proton.type.messaging.Outcome
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000033L), Symbol.valueOf("amqp:declared:list"), 
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000033L);
    private final DeclaredWrapper _wrapper = new DeclaredWrapper();
    
    private Binary _txnId;

    public Binary getTxnId()
    {
        return _txnId;
    }

    public void setTxnId(Binary txnId)
    {
        if( txnId == null )
        {
            throw new NullPointerException("the txn-id field is mandatory");
        }

        _txnId = txnId;
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
                return _txnId;            
        }

        throw new IllegalStateException("Unknown index " + index);

    }

    public int size()
    {
        return 1;        

    }


    public final class DeclaredWrapper extends AbstractList
    {

        @Override
        public Object get(final int index)
        {
            return Declared.this.get(index);
        }

        @Override
        public int size()
        {
            return Declared.this.size();
        }
    }

    private static class DeclaredConstructor implements DescribedTypeConstructor<Declared>
    {
        public Declared newInstance(Object described)
        {
            List l = (List) described;

            Declared o = new Declared();

            if(l.size() <= 0)
            {
                throw new DecodeException("The txn-id field cannot be omitted");
            }

            switch(1 - l.size())
            {

                case 0:
                    o.setTxnId( (Binary) l.get( 0 ) );
            }


            return o;
        }

        public Class<Declared> getTypeClass()
        {
            return Declared.class;
        }
    }


    public static void register(Decoder decoder)
    {
        DeclaredConstructor constructor = new DeclaredConstructor();
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, constructor);
        }
    }
}
  