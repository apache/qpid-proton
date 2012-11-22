
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


public class Discharge
      implements DescribedType 
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000032L), Symbol.valueOf("amqp:discharge:list"), 
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000032L);
    private final DischargeWrapper _wrapper = new DischargeWrapper();
    
    private Binary _txnId;
    private Boolean _fail;

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

    public Boolean getFail()
    {
        return _fail;
    }

    public void setFail(Boolean fail)
    {
        _fail = fail;
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
            case 1:
                return _fail;            
        }

        throw new IllegalStateException("Unknown index " + index);

    }

    public int size()
    {
        return _fail != null 
                  ? 2 
                  : 1;        

    }


    public final class DischargeWrapper extends AbstractList
    {

        @Override
        public Object get(final int index)
        {
            return Discharge.this.get(index);
        }

        @Override
        public int size()
        {
            return Discharge.this.size();
        }
    }

    private static class DischargeConstructor implements DescribedTypeConstructor<Discharge>
    {
        public Discharge newInstance(Object described)
        {
            List l = (List) described;

            Discharge o = new Discharge();

            if(l.size() <= 0)
            {
                throw new DecodeException("The txn-id field cannot be omitted");
            }

            switch(2 - l.size())
            {

                case 0:
                    o.setFail( (Boolean) l.get( 1 ) );
                case 1:
                    o.setTxnId( (Binary) l.get( 0 ) );
            }


            return o;
        }

        public Class<Discharge> getTypeClass()
        {
            return Discharge.class;
        }
    }


    public static void register(Decoder decoder)
    {
        DischargeConstructor constructor = new DischargeConstructor();
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, constructor);
        }
    }
}
  