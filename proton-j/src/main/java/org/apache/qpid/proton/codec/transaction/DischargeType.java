
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

import java.util.AbstractList;
import java.util.List;
import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.transaction.Discharge;
import org.apache.qpid.proton.codec.AbstractDescribedType;
import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.codec.EncoderImpl;


public class DischargeType extends AbstractDescribedType<Discharge,List> implements DescribedTypeConstructor<Discharge>
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000032L), Symbol.valueOf("amqp:discharge:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000032L);

    private DischargeType(EncoderImpl encoder)
    {
        super(encoder);
    }

    public UnsignedLong getDescriptor()
    {
        return DESCRIPTOR;
    }

    @Override
    protected List wrap(Discharge val)
    {
        return new DischargeWrapper(val);
    }


    public static class DischargeWrapper extends AbstractList
    {

        private Discharge _discharge;

        public DischargeWrapper(Discharge discharge)
        {
            _discharge = discharge;
        }

        public Object get(final int index)
        {

            switch(index)
            {
                case 0:
                    return _discharge.getTxnId();
                case 1:
                    return _discharge.getFail();
            }

            throw new IllegalStateException("Unknown index " + index);

        }

        public int size()
        {
            return _discharge.getFail() != null
                      ? 2
                      : 1;

        }

    }

    public Discharge newInstance(Object described)
    {
        List l = (List) described;

        Discharge o = new Discharge();

        if(l.isEmpty())
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



    public static void register(Decoder decoder, EncoderImpl encoder)
    {
        DischargeType type = new DischargeType(encoder);
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, type);
        }
        encoder.register(type);
    }
}
  