
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


package org.apache.qpid.proton.codec.transport;

import java.util.AbstractList;
import java.util.List;
import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.UnsignedByte;
import org.apache.qpid.proton.amqp.UnsignedInteger;
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.transport.DeliveryState;
import org.apache.qpid.proton.amqp.transport.ReceiverSettleMode;
import org.apache.qpid.proton.amqp.transport.Transfer;
import org.apache.qpid.proton.codec.AbstractDescribedType;
import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.codec.EncoderImpl;


public final class TransferType extends AbstractDescribedType<Transfer,List> implements DescribedTypeConstructor<Transfer>
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000014L), Symbol.valueOf("amqp:transfer:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000014L);

    private TransferType(EncoderImpl encoder)
    {
        super(encoder);
    }


    public UnsignedLong getDescriptor()
    {
        return DESCRIPTOR;
    }

    @Override
    protected List wrap(Transfer val)
    {
        return new TransferWrapper(val);
    }


    public static class TransferWrapper extends AbstractList
    {

        private Transfer _transfer;

        public TransferWrapper(Transfer transfer)
        {
            _transfer = transfer;
        }

        public Object get(final int index)
        {

            switch(index)
            {
                case 0:
                    return _transfer.getHandle();
                case 1:
                    return _transfer.getDeliveryId();
                case 2:
                    return _transfer.getDeliveryTag();
                case 3:
                    return _transfer.getMessageFormat();
                case 4:
                    return _transfer.getSettled();
                case 5:
                    return _transfer.getMore();
                case 6:
                    return _transfer.getRcvSettleMode() == null ? null : _transfer.getRcvSettleMode().getValue();
                case 7:
                    return _transfer.getState();
                case 8:
                    return _transfer.getResume();
                case 9:
                    return _transfer.getAborted();
                case 10:
                    return _transfer.getBatchable();
            }

            throw new IllegalStateException("Unknown index " + index);

        }

        public int size()
        {
            return _transfer.getBatchable()
                      ? 11
                      : _transfer.getAborted()
                      ? 10
                      : _transfer.getResume()
                      ? 9
                      : _transfer.getState() != null
                      ? 8
                      : _transfer.getRcvSettleMode() != null
                      ? 7
                      : _transfer.getMore()
                      ? 6
                      : _transfer.getSettled() != null
                      ? 5
                      : _transfer.getMessageFormat() != null
                      ? 4
                      : _transfer.getDeliveryTag() != null
                      ? 3
                      : _transfer.getDeliveryId() != null
                      ? 2
                      : 1;

        }

    }

        public Transfer newInstance(Object described)
        {
            List l = (List) described;

            Transfer o = new Transfer();

            if(l.isEmpty())
            {
                throw new DecodeException("The handle field cannot be omitted");
            }

            switch(11 - l.size())
            {

                case 0:
                    Boolean batchable = (Boolean) l.get(10);
                    o.setBatchable(batchable == null ? false : batchable);
                case 1:
                    Boolean aborted = (Boolean) l.get(9);
                    o.setAborted(aborted == null ? false : aborted);
                case 2:
                    Boolean resume = (Boolean) l.get(8);
                    o.setResume(resume == null ? false : resume);
                case 3:
                    o.setState( (DeliveryState) l.get( 7 ) );
                case 4:
                    UnsignedByte receiverSettleMode = (UnsignedByte) l.get(6);
                    o.setRcvSettleMode(receiverSettleMode == null ? null : ReceiverSettleMode.values()[receiverSettleMode.intValue()]);
                case 5:
                    Boolean more = (Boolean) l.get(5);
                    o.setMore(more == null ? false : more );
                case 6:
                    o.setSettled( (Boolean) l.get( 4 ) );
                case 7:
                    o.setMessageFormat( (UnsignedInteger) l.get( 3 ) );
                case 8:
                    o.setDeliveryTag( (Binary) l.get( 2 ) );
                case 9:
                    o.setDeliveryId( (UnsignedInteger) l.get( 1 ) );
                case 10:
                    o.setHandle( (UnsignedInteger) l.get( 0 ) );
            }


            return o;
        }

        public Class<Transfer> getTypeClass()
        {
            return Transfer.class;
        }




    public static void register(Decoder decoder, EncoderImpl encoder)
    {
        TransferType type = new TransferType(encoder);
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, type);
        }
        encoder.register(type);
    }

}
  