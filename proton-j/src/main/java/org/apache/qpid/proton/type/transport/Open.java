
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


public class Open
      implements DescribedType , FrameBody
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000010L), Symbol.valueOf("amqp:open:list"), 
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000010L);
    private final OpenWrapper _wrapper = new OpenWrapper();
    
    private String _containerId;
    private String _hostname;
    private UnsignedInteger _maxFrameSize = UnsignedInteger.valueOf(0xffffffff);
    private UnsignedShort _channelMax = UnsignedShort.valueOf((short) 65535);
    private UnsignedInteger _idleTimeOut;
    private Symbol[] _outgoingLocales;
    private Symbol[] _incomingLocales;
    private Symbol[] _offeredCapabilities;
    private Symbol[] _desiredCapabilities;
    private Map _properties;

    public String getContainerId()
    {
        return _containerId;
    }

    public void setContainerId(String containerId)
    {
        if( containerId == null )
        {
            throw new NullPointerException("the container-id field is mandatory");
        }

        _containerId = containerId;
    }

    public String getHostname()
    {
        return _hostname;
    }

    public void setHostname(String hostname)
    {
        _hostname = hostname;
    }

    public UnsignedInteger getMaxFrameSize()
    {
        return _maxFrameSize;
    }

    public void setMaxFrameSize(UnsignedInteger maxFrameSize)
    {
        _maxFrameSize = maxFrameSize;
    }

    public UnsignedShort getChannelMax()
    {
        return _channelMax;
    }

    public void setChannelMax(UnsignedShort channelMax)
    {
        _channelMax = channelMax;
    }

    public UnsignedInteger getIdleTimeOut()
    {
        return _idleTimeOut;
    }

    public void setIdleTimeOut(UnsignedInteger idleTimeOut)
    {
        _idleTimeOut = idleTimeOut;
    }

    public Symbol[] getOutgoingLocales()
    {
        return _outgoingLocales;
    }

    public void setOutgoingLocales(Symbol... outgoingLocales)
    {
        _outgoingLocales = outgoingLocales;
    }

    public Symbol[] getIncomingLocales()
    {
        return _incomingLocales;
    }

    public void setIncomingLocales(Symbol... incomingLocales)
    {
        _incomingLocales = incomingLocales;
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
                return _containerId;
            case 1:
                return _hostname;
            case 2:
                return _maxFrameSize;
            case 3:
                return _channelMax;
            case 4:
                return _idleTimeOut;
            case 5:
                return _outgoingLocales;
            case 6:
                return _incomingLocales;
            case 7:
                return _offeredCapabilities;
            case 8:
                return _desiredCapabilities;
            case 9:
                return _properties;            
        }

        throw new IllegalStateException("Unknown index " + index);

    }

    public int size()
    {
        return _properties != null 
                  ? 10 
                  : _desiredCapabilities != null 
                  ? 9 
                  : _offeredCapabilities != null 
                  ? 8 
                  : _incomingLocales != null 
                  ? 7 
                  : _outgoingLocales != null 
                  ? 6 
                  : _idleTimeOut != null 
                  ? 5 
                  : (_channelMax != null && !_channelMax.equals(UnsignedShort.MAX_VALUE))
                  ? 4 
                  : (_maxFrameSize != null && !_maxFrameSize.equals(UnsignedInteger.MAX_VALUE))
                  ? 3 
                  : _hostname != null 
                  ? 2 
                  : 1;        

    }


    public final class OpenWrapper extends AbstractList
    {

        @Override
        public Object get(final int index)
        {
            return Open.this.get(index);
        }

        @Override
        public int size()
        {
            return Open.this.size();
        }
    }

    private static class OpenConstructor implements DescribedTypeConstructor<Open>
    {
        public Open newInstance(Object described)
        {
            List l = (List) described;

            Open o = new Open();

            if(l.size() <= 0)
            {
                throw new DecodeException("The container-id field cannot be omitted");
            }

            switch(10 - l.size())
            {

                case 0:
                    o.setProperties( (Map) l.get( 9 ) );
                case 1:
                    Object val1 = l.get( 8 );
                    if( val1 == null || val1.getClass().isArray() )
                    {
                        o.setDesiredCapabilities( (Symbol[]) val1 );
                    }
                    else
                    {
                        o.setDesiredCapabilities( (Symbol) val1 );
                    }
                case 2:
                    Object val2 = l.get( 7 );
                    if( val2 == null || val2.getClass().isArray() )
                    {
                        o.setOfferedCapabilities( (Symbol[]) val2 );
                    }
                    else
                    {
                        o.setOfferedCapabilities( (Symbol) val2 );
                    }
                case 3:
                    Object val3 = l.get( 6 );
                    if( val3 == null || val3.getClass().isArray() )
                    {
                        o.setIncomingLocales( (Symbol[]) val3 );
                    }
                    else
                    {
                        o.setIncomingLocales( (Symbol) val3 );
                    }
                case 4:
                    Object val4 = l.get( 5 );
                    if( val4 == null || val4.getClass().isArray() )
                    {
                        o.setOutgoingLocales( (Symbol[]) val4 );
                    }
                    else
                    {
                        o.setOutgoingLocales( (Symbol) val4 );
                    }
                case 5:
                    o.setIdleTimeOut( (UnsignedInteger) l.get( 4 ) );
                case 6:
                    UnsignedShort channelMax = (UnsignedShort) l.get(3);
                    o.setChannelMax(channelMax == null ? UnsignedShort.MAX_VALUE : channelMax);
                case 7:
                    UnsignedInteger maxFrameSize = (UnsignedInteger) l.get(2);
                    o.setMaxFrameSize(maxFrameSize == null ? UnsignedInteger.MAX_VALUE : maxFrameSize);
                case 8:
                    o.setHostname( (String) l.get( 1 ) );
                case 9:
                    o.setContainerId( (String) l.get( 0 ) );
            }


            return o;
        }

        public Class<Open> getTypeClass()
        {
            return Open.class;
        }
    }

    public <E> void invoke(FrameBodyHandler<E> handler, Binary payload, E context)
    {
        handler.handleOpen(this, payload, context);
    }


    public static void register(Decoder decoder)
    {
        OpenConstructor constructor = new OpenConstructor();
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, constructor);
        }
    }

    @Override
    public String toString()
    {
        return "Open{" +
               " containerId='" + _containerId + '\'' +
               ", hostname='" + _hostname + '\'' +
               ", maxFrameSize=" + _maxFrameSize +
               ", channelMax=" + _channelMax +
               ", idleTimeOut=" + _idleTimeOut +
               ", outgoingLocales=" + (_outgoingLocales == null ? null : Arrays.asList(_outgoingLocales)) +
               ", incomingLocales=" + (_incomingLocales == null ? null : Arrays.asList(_incomingLocales)) +
               ", offeredCapabilities=" + (_offeredCapabilities == null ? null : Arrays.asList(_offeredCapabilities)) +
               ", desiredCapabilities=" + (_desiredCapabilities == null ? null : Arrays.asList(_desiredCapabilities)) +
               ", properties=" + _properties +
               '}';
    }
}
  