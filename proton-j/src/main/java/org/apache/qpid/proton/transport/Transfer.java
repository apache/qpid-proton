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

package org.apache.qpid.proton.transport;

import java.util.List;

import org.apache.qpid.proton.codec2.DecodeException;
import org.apache.qpid.proton.codec2.DescribedTypeFactory;
import org.apache.qpid.proton.codec2.Encodable;
import org.apache.qpid.proton.codec2.Encoder;

public final class Transfer implements Encodable
{
    public final static long DESCRIPTOR_LONG = 0x0000000000000014L;

    public final static String DESCRIPTOR_STRING = "amqp:transfer:list";

    private int _handle;

    private int _deliveryId;

    private byte[] _deliveryTag;

    private int _messageFormat;

    private Boolean _settled;

    private boolean _more;

    private ReceiverSettleMode _rcvSettleMode;

    private DeliveryState _state;

    private boolean _resume;

    private boolean _aborted;

    private boolean _batchable;

    public int getHandle()
    {
        return _handle;
    }

    public void setHandle(int handle)
    {
        _handle = handle;
    }

    public int getDeliveryId()
    {
        return _deliveryId;
    }

    public void setDeliveryId(int deliveryId)
    {
        _deliveryId = deliveryId;
    }

    public byte[] getDeliveryTag()
    {
        return _deliveryTag;
    }

    public void setDeliveryTag(byte[] deliveryTag)
    {
        _deliveryTag = deliveryTag;
    }

    public int getMessageFormat()
    {
        return _messageFormat;
    }

    public void setMessageFormat(int messageFormat)
    {
        _messageFormat = messageFormat;
    }

    public Boolean getSettled()
    {
        return _settled;
    }

    public void setSettled(Boolean settled)
    {
        _settled = settled;
    }

    public boolean getMore()
    {
        return _more;
    }

    public void setMore(boolean more)
    {
        _more = more;
    }

    public ReceiverSettleMode getRcvSettleMode()
    {
        return _rcvSettleMode;
    }

    public void setRcvSettleMode(ReceiverSettleMode rcvSettleMode)
    {
        _rcvSettleMode = rcvSettleMode;
    }

    public DeliveryState getState()
    {
        return _state;
    }

    public void setState(DeliveryState state)
    {
        _state = state;
    }

    public boolean getResume()
    {
        return _resume;
    }

    public void setResume(boolean resume)
    {
        _resume = resume;
    }

    public boolean getAborted()
    {
        return _aborted;
    }

    public void setAborted(boolean aborted)
    {
        _aborted = aborted;
    }

    public boolean getBatchable()
    {
        return _batchable;
    }

    public void setBatchable(boolean batchable)
    {
        _batchable = batchable;
    }

    @Override
    public void encode(Encoder encoder)
    {
        encoder.putDescriptor();
        encoder.putUlong(DESCRIPTOR_LONG);
        encoder.putList();
        encoder.putUint(_handle);
        encoder.putUint(_deliveryId);
        encoder.putBinary(_deliveryTag, 0, _deliveryTag.length);
        encoder.putUint(_messageFormat);
        encoder.putBoolean(_settled);
        encoder.putBoolean(_more);
        encoder.putUbyte(_rcvSettleMode.getValue());
        if (_state == null)
        {
            encoder.putNull();
        }
        else
        {
            _state.encode(encoder);
        }
        encoder.putBoolean(_resume);
        encoder.putBoolean(_aborted);
        encoder.putBoolean(_batchable);
        encoder.end();
    }

    public static final class Factory implements DescribedTypeFactory
    {
        @SuppressWarnings("unchecked")
        public Object create(Object in) throws DecodeException
        {
            List<Object> l = (List<Object>) in;
            Transfer transfer = new Transfer();

            switch (11 - l.size())
            {
            case 0:
                transfer.setBatchable(l.get(10) == null ? false : (boolean) l.get(10));
            case 1:
                transfer.setAborted(l.get(9) == null ? false : (boolean) l.get(9));
            case 2:
                transfer.setResume(l.get(8) == null ? false : (boolean) l.get(8));
            case 3:
                transfer.setState((DeliveryState) l.get(7));
            case 4:
                transfer.setRcvSettleMode(l.get(6) == null ? null : ReceiverSettleMode.values()[(int) l.get(6)]);
            case 5:
                transfer.setMore(l.get(5) == null ? false : (boolean) l.get(5));
            case 6:
                transfer.setSettled((boolean) l.get(4));
            case 7:
                transfer.setMessageFormat((int) l.get(3));
            case 8:
                transfer.setDeliveryTag((byte[]) l.get(2));
            case 9:
                transfer.setDeliveryId((int) l.get(1));
            case 10:
                transfer.setHandle((int) l.get(0));
            }

            return transfer;
        }
    }

    @Override
    public String toString()
    {
        return "Transfer{" +
               "handle=" + _handle +
               ", deliveryId=" + _deliveryId +
               ", deliveryTag=" + _deliveryTag +
               ", messageFormat=" + _messageFormat +
               ", settled=" + _settled +
               ", more=" + _more +
               ", rcvSettleMode=" + _rcvSettleMode +
               ", state=" + _state +
               ", resume=" + _resume +
               ", aborted=" + _aborted +
               ", batchable=" + _batchable +
               '}';
    }
}