
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

import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.type.Binary;
import org.apache.qpid.proton.type.DescribedType;
import org.apache.qpid.proton.type.Symbol;
import org.apache.qpid.proton.type.UnsignedInteger;
import org.apache.qpid.proton.type.UnsignedLong;
import org.apache.qpid.proton.type.UnsignedShort;

import java.util.AbstractList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;


public class Begin
      implements DescribedType , FrameBody
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000011L), Symbol.valueOf("amqp:begin:list"), 
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000011L);
    private final BeginWrapper _wrapper = new BeginWrapper();
    
    private UnsignedShort _remoteChannel;
    private UnsignedInteger _nextOutgoingId;
    private UnsignedInteger _incomingWindow;
    private UnsignedInteger _outgoingWindow;
    private UnsignedInteger _handleMax = UnsignedInteger.valueOf(0xffffffff);
    private Symbol[] _offeredCapabilities;
    private Symbol[] _desiredCapabilities;
    private Map _properties;

    public UnsignedShort getRemoteChannel()
    {
        return _remoteChannel;
    }

    public void setRemoteChannel(UnsignedShort remoteChannel)
    {
        _remoteChannel = remoteChannel;
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

    public UnsignedInteger getHandleMax()
    {
        return _handleMax;
    }

    public void setHandleMax(UnsignedInteger handleMax)
    {
        _handleMax = handleMax;
    }

    public Symbol[] getOfferedCapabilities()
    {
        return _offeredCapabilities;
    }

    public void setOfferedCapabilities(Symbol... offeredCapabilities)
    {
        _offeredCapabilities = offeredCapabilities;
    }

    public Symbol[] getDesiredCapabilities()
    {
        return _desiredCapabilities;
    }

    public void setDesiredCapabilities(Symbol... desiredCapabilities)
    {
        _desiredCapabilities = desiredCapabilities;
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
                return _remoteChannel;
            case 1:
                return _nextOutgoingId;
            case 2:
                return _incomingWindow;
            case 3:
                return _outgoingWindow;
            case 4:
                return _handleMax;
            case 5:
                return _offeredCapabilities;
            case 6:
                return _desiredCapabilities;
            case 7:
                return _properties;            
        }

        throw new IllegalStateException("Unknown index " + index);

    }

    public int size()
    {
        return _properties != null 
                  ? 8 
                  : _desiredCapabilities != null 
                  ? 7 
                  : _offeredCapabilities != null 
                  ? 6 
                  : (_handleMax != null && !_handleMax.equals(UnsignedInteger.MAX_VALUE))
                  ? 5 
                  : 4;        

    }


    public final class BeginWrapper extends AbstractList
    {

        @Override
        public Object get(final int index)
        {
            return Begin.this.get(index);
        }

        @Override
        public int size()
        {
            return Begin.this.size();
        }
    }

    private static class BeginConstructor implements DescribedTypeConstructor<Begin>
    {
        public Begin newInstance(Object described)
        {
            List l = (List) described;

            Begin o = new Begin();

            if(l.size() <= 3)
            {
                throw new DecodeException("The outgoing-window field cannot be omitted");
            }

            switch(8 - l.size())
            {

                case 0:
                    o.setProperties( (Map) l.get( 7 ) );
                case 1:
                    Object val1 = l.get( 6 );
                    if( val1 == null || val1.getClass().isArray() )
                    {
                        o.setDesiredCapabilities( (Symbol[]) val1 );
                    }
                    else
                    {
                        o.setDesiredCapabilities( (Symbol) val1 );
                    }
                case 2:
                    Object val2 = l.get( 5 );
                    if( val2 == null || val2.getClass().isArray() )
                    {
                        o.setOfferedCapabilities( (Symbol[]) val2 );
                    }
                    else
                    {
                        o.setOfferedCapabilities( (Symbol) val2 );
                    }
                case 3:
                    UnsignedInteger handleMax = (UnsignedInteger) l.get(4);
                    o.setHandleMax(handleMax == null ? UnsignedInteger.MAX_VALUE : handleMax);
                case 4:
                    o.setOutgoingWindow( (UnsignedInteger) l.get( 3 ) );
                case 5:
                    o.setIncomingWindow( (UnsignedInteger) l.get( 2 ) );
                case 6:
                    o.setNextOutgoingId( (UnsignedInteger) l.get( 1 ) );
                case 7:
                    o.setRemoteChannel( (UnsignedShort) l.get( 0 ) );
            }


            return o;
        }

        public Class<Begin> getTypeClass()
        {
            return Begin.class;
        }
    }

    public <E> void invoke(FrameBodyHandler<E> handler, Binary payload, E context)
    {
        handler.handleBegin(this, payload, context);
    }


    public static void register(Decoder decoder)
    {
        BeginConstructor constructor = new BeginConstructor();
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, constructor);
        }
    }

    @Override
    public String toString()
    {
        return "Begin{" +
               "remoteChannel=" + _remoteChannel +
               ", nextOutgoingId=" + _nextOutgoingId +
               ", incomingWindow=" + _incomingWindow +
               ", outgoingWindow=" + _outgoingWindow +
               ", handleMax=" + _handleMax +
               ", offeredCapabilities=" + (_offeredCapabilities == null ? null : Arrays.asList(_offeredCapabilities)) +
               ", desiredCapabilities=" + (_desiredCapabilities == null ? null : Arrays.asList(_desiredCapabilities)) +
               ", properties=" + _properties +
               '}';
    }
}
  