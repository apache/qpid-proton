
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
import org.apache.qpid.proton.amqp.messaging.Outcome;
import org.apache.qpid.proton.amqp.transaction.TransactionalState;
import org.apache.qpid.proton.codec.AbstractDescribedType;
import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.codec.EncoderImpl;

public class TransactionalStateType extends AbstractDescribedType<TransactionalState,List> implements DescribedTypeConstructor<TransactionalState>
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000034L), Symbol.valueOf("amqp:transactional-state:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000034L);

    private TransactionalStateType(EncoderImpl encoder)
    {
        super(encoder);
    }

    public UnsignedLong getDescriptor()
    {
        return DESCRIPTOR;
    }

    @Override
    protected List wrap(TransactionalState val)
    {
        return new TransactionalStateWrapper(val);
    }

    public static class TransactionalStateWrapper extends AbstractList
    {

        private TransactionalState _transactionalState;

        public TransactionalStateWrapper(TransactionalState transactionalState)
        {
            _transactionalState = transactionalState;
        }

        public Object get(final int index)
        {

            switch(index)
            {
                case 0:
                    return _transactionalState.getTxnId();
                case 1:
                    return _transactionalState.getOutcome();
            }

            throw new IllegalStateException("Unknown index " + index);

        }

        public int size()
        {
            return _transactionalState.getOutcome() != null
                      ? 2
                      : 1;

        }


    }

    public TransactionalState newInstance(Object described)
    {
        List l = (List) described;

        TransactionalState o = new TransactionalState();

        if(l.isEmpty())
        {
            throw new DecodeException("The txn-id field cannot be omitted");
        }

        switch(2 - l.size())
        {

            case 0:
                o.setOutcome( (Outcome) l.get( 1 ) );
            case 1:
                o.setTxnId( (Binary) l.get( 0 ) );
        }


        return o;
    }

    public Class<TransactionalState> getTypeClass()
    {
        return TransactionalState.class;
    }



    public static void register(Decoder decoder, EncoderImpl encoder)
    {
        TransactionalStateType type = new TransactionalStateType(encoder);
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, type);
        }
        encoder.register(type);
    }
}
  