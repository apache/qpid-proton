
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
import java.util.Map;
import java.util.List;
import java.util.AbstractList;


import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.type.*;


public class Flow
      implements DescribedType , FrameBody
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000013L), Symbol.valueOf("amqp:flow:list"), 
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000013L);
    private final FlowWrapper _wrapper = new FlowWrapper();
    
    private UnsignedInteger _nextIncomingId;
    private UnsignedInteger _incomingWindow;
    private UnsignedInteger _nextOutgoingId;
    private UnsignedInteger _outgoingWindow;
    private UnsignedInteger _handle;
    private UnsignedInteger _deliveryCount;
    private UnsignedInteger _linkCredit;
    private UnsignedInteger _available;
    private boolean _drain;
    private boolean _echo;
    private Map _properties;

    public UnsignedInteger getNextIncomingId()
    {
        return _nextIncomingId;
    }

    public void setNextIncomingId(UnsignedInteger nextIncomingId)
    {
        _nextIncomingId = nextIncomingId;
    }

    public UnsignedInteger getIncomingWindow()
    {
        return _incomingWindow;
    }

    public void setIncomingWindow(UnsignedInteger incomingWindow)
    {
        if( incomingWindow == null )
        {
            throw new NullPointerException("the incoming-window field is mandatory");
        }

        _incomingWindow = incomingWindow;
    }

    public UnsignedInteger getNextOutgoingId()
    {
        return _nextOutgoingId;
    }

    public void setNextOutgoingId(UnsignedInteger nextOutgoingId)
    {
        if( nextOutgoingId == null )
        {
            throw new NullPointerException("the next-outgoing-id field is mandatory");
        }

        _nextOutgoingId = nextOutgoingId;
    }

    public UnsignedInteger getOutgoingWindow()
    {
        return _outgoingWindow;
    }

    public void setOutgoingWindow(UnsignedInteger outgoingWindow)
    {
        if( outgoingWindow == null )
        {
            throw new NullPointerException("the outgoing-window field is mandatory");
        }

        _outgoingWindow = outgoingWindow;
    }

    public UnsignedInteger getHandle()
    {
        return _handle;
    }

    public void setHandle(UnsignedInteger handle)
    {
        _handle = handle;
    }

    public UnsignedInteger getDeliveryCount()
    {
        return _deliveryCount;
    }

    public void setDeliveryCount(UnsignedInteger deliveryCount)
    {
        _deliveryCount = deliveryCount;
    }

    public UnsignedInteger getLinkCredit()
    {
        return _linkCredit;
    }

    public void setLinkCredit(UnsignedInteger linkCredit)
    {
        _linkCredit = linkCredit;
    }

    public UnsignedInteger getAvailable()
    {
        return _available;
    }

    public void setAvailable(UnsignedInteger available)
    {
        _available = available;
    }

    public boolean getDrain()
    {
        return _drain;
    }

    public void setDrain(boolean drain)
    {
        _drain = drain;
    }

    public boolean getEcho()
    {
        return _echo;
    }

    public void setEcho(boolean echo)
    {
        _echo = echo;
    }

    public Map getProperties()
    {
        return _properties;
    }

    public void setProperties(Map properties)
    {
        _properties = properties;
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
                return _nextIncomingId;
            case 1:
                return _incomingWindow;
            case 2:
                return _nextOutgoingId;
            case 3:
                return _outgoingWindow;
            case 4:
                return _handle;
            case 5:
                return _deliveryCount;
            case 6:
                return _linkCredit;
            case 7:
                return _available;
            case 8:
                return _drain;
            case 9:
                return _echo;
            case 10:
                return _properties;            
        }

        throw new IllegalStateException("Unknown index " + index);

    }

    public int size()
    {
        return _properties != null 
                  ? 11 
                  : (_echo != false)
                  ? 10 
                  : (_drain != false)
                  ? 9 
                  : _available != null 
                  ? 8 
                  : _linkCredit != null 
                  ? 7 
                  : _deliveryCount != null 
                  ? 6 
                  : _handle != null 
                  ? 5 
                  : 4;        

    }


    public final class FlowWrapper extends AbstractList
    {

        @Override
        public Object get(final int index)
        {
            return Flow.this.get(index);
        }

        @Override
        public int size()
        {
            return Flow.this.size();
        }
    }

    private static class FlowConstructor implements DescribedTypeConstructor<Flow>
    {
        public Flow newInstance(Object described)
        {
            List l = (List) described;

            Flow o = new Flow();

            if(l.size() <= 3)
            {
                throw new DecodeException("The outgoing-window field cannot be omitted");
            }

            switch(11 - l.size())
            {

                case 0:
                    o.setProperties( (Map) l.get( 10 ) );
                case 1:
                    Boolean echo = (Boolean) l.get(9);
                    o.setEcho(echo == null ? false : echo);
                case 2:
                    Boolean drain = (Boolean) l.get(8);
                    o.setDrain(drain == null ? false : drain );
                case 3:
                    o.setAvailable( (UnsignedInteger) l.get( 7 ) );
                case 4:
                    o.setLinkCredit( (UnsignedInteger) l.get( 6 ) );
                case 5:
                    o.setDeliveryCount( (UnsignedInteger) l.get( 5 ) );
                case 6:
                    o.setHandle( (UnsignedInteger) l.get( 4 ) );
                case 7:
                    o.setOutgoingWindow( (UnsignedInteger) l.get( 3 ) );
                case 8:
                    o.setNextOutgoingId( (UnsignedInteger) l.get( 2 ) );
                case 9:
                    o.setIncomingWindow( (UnsignedInteger) l.get( 1 ) );
                case 10:
                    o.setNextIncomingId( (UnsignedInteger) l.get( 0 ) );
            }


            return o;
        }

        public Class<Flow> getTypeClass()
        {
            return Flow.class;
        }
    }

    public <E> void invoke(FrameBodyHandler<E> handler, Binary payload, E context)
    {
        handler.handleFlow(this, payload, context);
    }


    public static void register(Decoder decoder)
    {
        FlowConstructor constructor = new FlowConstructor();
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, constructor);
        }
    }

    @Override
    public String toString()
    {
        return "Flow{" +
               "nextIncomingId=" + _nextIncomingId +
               ", incomingWindow=" + _incomingWindow +
               ", nextOutgoingId=" + _nextOutgoingId +
               ", outgoingWindow=" + _outgoingWindow +
               ", handle=" + _handle +
               ", deliveryCount=" + _deliveryCount +
               ", linkCredit=" + _linkCredit +
               ", available=" + _available +
               ", drain=" + _drain +
               ", echo=" + _echo +
               ", properties=" + _properties +
               '}';
    }
}
  