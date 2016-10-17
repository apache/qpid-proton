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
import org.apache.qpid.proton.codec.DecoderImpl;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.codec.EncoderImpl;
import org.apache.qpid.proton.codec.ListType;
import org.apache.qpid.proton.codec.ReadableBuffer;
import org.apache.qpid.proton.codec.TypeConstructor;


public final class TransferType extends AbstractDescribedType<Transfer, List> implements DescribedTypeConstructor<Transfer>
{
    private static final Object[] DESCRIPTORS =
        {
            UnsignedLong.valueOf(0x0000000000000014L), Symbol.valueOf("amqp:transfer:list"),
        };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000014L);

    private final DecoderImpl _decoder;

    public TransferType(DecoderImpl decoder, EncoderImpl encoder)
    {
        super(encoder);
        this._decoder = decoder;
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

            switch (index)
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
    public Transfer newInstance(ReadableBuffer buffer, TypeConstructor constructor)
    {
        ListType.ListEncoding listEncoding = (ListType.ListEncoding)constructor;

        // This is for the prototype only, we should use the ListType directly here
        int size = listEncoding.readCount(buffer, _decoder);

        Transfer o = new Transfer();

        if (size == 0)
        {
            throw new DecodeException("The handle field cannot be omitted");
        }

        for (int i = 0; i < size; i++)
        {
            Object elementValue = listEncoding.readElement(buffer,  _decoder);
            switch (i)
            {

                case 0:
                    o.setHandle((UnsignedInteger) elementValue);
                    break;

                case 1:
                    o.setDeliveryId((UnsignedInteger) elementValue);
                    break;

                case 2:
                    o.setDeliveryTag((Binary) elementValue);
                    break;
                case 3:
                    o.setMessageFormat((UnsignedInteger) elementValue);
                    break;
                case 4:
                    o.setSettled((Boolean) elementValue);
                    break;
                case 5:
                    Boolean more = (Boolean) elementValue;
                    o.setMore(more == null ? false : more);
                    break;
                case 6:
                    UnsignedByte receiverSettleMode = (UnsignedByte) elementValue;
                    o.setRcvSettleMode(receiverSettleMode == null ? null : ReceiverSettleMode.values()[receiverSettleMode.intValue()]);
                    break;
                case 7:
                    o.setState((DeliveryState) elementValue);
                    break;
                case 8:
                    Boolean resume = (Boolean) elementValue;
                    o.setResume(resume == null ? false : resume);
                    break;
                case 9:
                    Boolean aborted = (Boolean) elementValue;
                    o.setAborted(aborted == null ? false : aborted);
                    break;
                case 10:
                    Boolean batchable = (Boolean) elementValue;
                    o.setBatchable(batchable == null ? false : batchable);
                    break;
            }

        }
        return o;
    }

    public Class<Transfer> getTypeClass()
    {
        return Transfer.class;
    }


    public static void register(DecoderImpl decoder, EncoderImpl encoder)
    {
        TransferType type = new TransferType(decoder, encoder);
        for (Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, type);
        }
        encoder.register(type);
    }

}
