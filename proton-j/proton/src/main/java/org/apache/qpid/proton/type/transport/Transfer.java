
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


package org.apache.qpid.proton.type.transport;
import java.util.List;
import java.util.AbstractList;


import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.type.*;


public class Transfer
      implements DescribedType , FrameBody
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000014L), Symbol.valueOf("amqp:transfer:list"), 
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000014L);
    private final TransferWrapper _wrapper = new TransferWrapper();
    
    private UnsignedInteger _handle;
    private UnsignedInteger _deliveryId;
    private Binary _deliveryTag;
    private UnsignedInteger _messageFormat;
    private Boolean _settled;
    private boolean _more;
    private UnsignedByte _rcvSettleMode;
    private DeliveryState _state;
    private boolean _resume;
    private boolean _aborted;
    private boolean _batchable;

    public UnsignedInteger getHandle()
    {
        return _handle;
    }

    public void setHandle(UnsignedInteger handle)
    {
        if( handle == null )
        {
            throw new NullPointerException("the handle field is mandatory");
        }

        _handle = handle;
    }

    public UnsignedInteger getDeliveryId()
    {
        return _deliveryId;
    }

    public void setDeliveryId(UnsignedInteger deliveryId)
    {
        _deliveryId = deliveryId;
    }

    public Binary getDeliveryTag()
    {
        return _deliveryTag;
    }

    public void setDeliveryTag(Binary deliveryTag)
    {
        _deliveryTag = deliveryTag;
    }

    public UnsignedInteger getMessageFormat()
    {
        return _messageFormat;
    }

    public void setMessageFormat(UnsignedInteger messageFormat)
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

    public UnsignedByte getRcvSettleMode()
    {
        return _rcvSettleMode;
    }

    public void setRcvSettleMode(UnsignedByte rcvSettleMode)
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
                return _handle;
            case 1:
                return _deliveryId;
            case 2:
                return _deliveryTag;
            case 3:
                return _messageFormat;
            case 4:
                return _settled;
            case 5:
                return _more;
            case 6:
                return _rcvSettleMode;
            case 7:
                return _state;
            case 8:
                return _resume;
            case 9:
                return _aborted;
            case 10:
                return _batchable;            
        }

        throw new IllegalStateException("Unknown index " + index);

    }

    public int size()
    {
        return (_batchable != false)
                  ? 11 
                  : (_aborted != false)
                  ? 10 
                  : (_resume != false)
                  ? 9 
                  : _state != null 
                  ? 8 
                  : _rcvSettleMode != null 
                  ? 7 
                  : (_more != false)
                  ? 6 
                  : _settled != null 
                  ? 5 
                  : _messageFormat != null 
                  ? 4 
                  : _deliveryTag != null 
                  ? 3 
                  : _deliveryId != null 
                  ? 2 
                  : 1;        

    }


    public final class TransferWrapper extends AbstractList
    {

        @Override
        public Object get(final int index)
        {
            return Transfer.this.get(index);
        }

        @Override
        public int size()
        {
            return Transfer.this.size();
        }
    }

    private static class TransferConstructor implements DescribedTypeConstructor<Transfer>
    {
        public Transfer newInstance(Object described)
        {
            List l = (List) described;

            Transfer o = new Transfer();

            if(l.size() <= 0)
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
                    o.setRcvSettleMode( (UnsignedByte) l.get( 6 ) );
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
    }

    public <E> void invoke(FrameBodyHandler<E> handler, Binary payload, E context)
    {
        handler.handleTransfer(this, payload, context);
    }


    public static void register(Decoder decoder)
    {
        TransferConstructor constructor = new TransferConstructor();
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, constructor);
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
  