
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
import org.apache.qpid.proton.type.UnsignedByte;
import org.apache.qpid.proton.type.UnsignedInteger;
import org.apache.qpid.proton.type.UnsignedLong;

import java.util.AbstractList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;


public class Attach
      implements DescribedType , FrameBody
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000012L), Symbol.valueOf("amqp:attach:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000012L);
    private final AttachWrapper _wrapper = new AttachWrapper();

    private String _name;
    private UnsignedInteger _handle;
    private boolean _role;
    private UnsignedByte _sndSettleMode = UnsignedByte.valueOf((byte) 2);
    private UnsignedByte _rcvSettleMode = UnsignedByte.valueOf((byte) 0);
    private Source _source;
    private Target _target;
    private Map _unsettled;
    private boolean _incompleteUnsettled;
    private UnsignedInteger _initialDeliveryCount;
    private UnsignedLong _maxMessageSize;
    private Symbol[] _offeredCapabilities;
    private Symbol[] _desiredCapabilities;
    private Map _properties;

    public String getName()
    {
        return _name;
    }

    public void setName(String name)
    {
        if( name == null )
        {
            throw new NullPointerException("the name field is mandatory");
        }

        _name = name;
    }

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

    public boolean getRole()
    {
        return _role;
    }

    public void setRole(boolean role)
    {
        _role = role;
    }

    public UnsignedByte getSndSettleMode()
    {
        return _sndSettleMode;
    }

    public void setSndSettleMode(UnsignedByte sndSettleMode)
    {
        _sndSettleMode = sndSettleMode;
    }

    public UnsignedByte getRcvSettleMode()
    {
        return _rcvSettleMode;
    }

    public void setRcvSettleMode(UnsignedByte rcvSettleMode)
    {
        _rcvSettleMode = rcvSettleMode;
    }

    public Source getSource()
    {
        return _source;
    }

    public void setSource(Source source)
    {
        _source = source;
    }

    public Target getTarget()
    {
        return _target;
    }

    public void setTarget(Target target)
    {
        _target = target;
    }

    public Map getUnsettled()
    {
        return _unsettled;
    }

    public void setUnsettled(Map unsettled)
    {
        _unsettled = unsettled;
    }

    public boolean getIncompleteUnsettled()
    {
        return _incompleteUnsettled;
    }

    public void setIncompleteUnsettled(boolean incompleteUnsettled)
    {
        _incompleteUnsettled = incompleteUnsettled;
    }

    public UnsignedInteger getInitialDeliveryCount()
    {
        return _initialDeliveryCount;
    }

    public void setInitialDeliveryCount(UnsignedInteger initialDeliveryCount)
    {
        _initialDeliveryCount = initialDeliveryCount;
    }

    public UnsignedLong getMaxMessageSize()
    {
        return _maxMessageSize;
    }

    public void setMaxMessageSize(UnsignedLong maxMessageSize)
    {
        _maxMessageSize = maxMessageSize;
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
                return _name;
            case 1:
                return _handle;
            case 2:
                return _role;
            case 3:
                return _sndSettleMode;
            case 4:
                return _rcvSettleMode;
            case 5:
                return _source;
            case 6:
                return _target;
            case 7:
                return _unsettled;
            case 8:
                return _incompleteUnsettled;
            case 9:
                return _initialDeliveryCount;
            case 10:
                return _maxMessageSize;
            case 11:
                return _offeredCapabilities;
            case 12:
                return _desiredCapabilities;
            case 13:
                return _properties;
        }

        throw new IllegalStateException("Unknown index " + index);

    }

    public int size()
    {
        return _properties != null
                  ? 14
                  : _desiredCapabilities != null
                  ? 13
                  : _offeredCapabilities != null
                  ? 12
                  : _maxMessageSize != null
                  ? 11
                  : _initialDeliveryCount != null
                  ? 10
                  : (_incompleteUnsettled != false)
                  ? 9
                  : _unsettled != null
                  ? 8
                  : _target != null
                  ? 7
                  : _source != null
                  ? 6
                  : (_rcvSettleMode != null && !_rcvSettleMode.equals(ReceiverSettleMode.FIRST))
                  ? 5
                  : (_sndSettleMode != null && !_sndSettleMode.equals(SenderSettleMode.MIXED))
                  ? 4
                  : 3;

    }


    public final class AttachWrapper extends AbstractList
    {

        @Override
        public Object get(final int index)
        {
            return Attach.this.get(index);
        }

        @Override
        public int size()
        {
            return Attach.this.size();
        }
    }

    private static class AttachConstructor implements DescribedTypeConstructor<Attach>
    {
        public Attach newInstance(Object described)
        {
            List l = (List) described;

            Attach o = new Attach();

            if(l.size() <= 2)
            {
                throw new DecodeException("The role field cannot be omitted");
            }

            switch(14 - l.size())
            {

                case 0:
                    o.setProperties( (Map) l.get( 13 ) );
                case 1:
                    Object val1 = l.get( 12 );
                    if( val1 == null || val1.getClass().isArray() )
                    {
                        o.setDesiredCapabilities( (Symbol[]) val1 );
                    }
                    else
                    {
                        o.setDesiredCapabilities( (Symbol) val1 );
                    }
                case 2:
                    Object val2 = l.get( 11 );
                    if( val2 == null || val2.getClass().isArray() )
                    {
                        o.setOfferedCapabilities( (Symbol[]) val2 );
                    }
                    else
                    {
                        o.setOfferedCapabilities( (Symbol) val2 );
                    }
                case 3:
                    o.setMaxMessageSize( (UnsignedLong) l.get( 10 ) );
                case 4:
                    o.setInitialDeliveryCount( (UnsignedInteger) l.get( 9 ) );
                case 5:
                    Boolean incompleteUnsettled = (Boolean) l.get(8);
                    o.setIncompleteUnsettled(incompleteUnsettled == null ? false : incompleteUnsettled);
                case 6:
                    o.setUnsettled( (Map) l.get( 7 ) );
                case 7:
                    o.setTarget( (Target) l.get( 6 ) );
                case 8:
                    o.setSource( (Source) l.get( 5 ) );
                case 9:
                    UnsignedByte rcvSettleMode = (UnsignedByte) l.get(4);
                    o.setRcvSettleMode(rcvSettleMode == null ? ReceiverSettleMode.FIRST : rcvSettleMode);
                case 10:
                    UnsignedByte sndSettleMode = (UnsignedByte) l.get(3);
                    o.setSndSettleMode(sndSettleMode == null ? SenderSettleMode.MIXED : sndSettleMode);
                case 11:
                    o.setRole( (Boolean) l.get( 2 ) );
                case 12:
                    o.setHandle( (UnsignedInteger) l.get( 1 ) );
                case 13:
                    o.setName( (String) l.get( 0 ) );
            }


            return o;
        }

        public Class<Attach> getTypeClass()
        {
            return Attach.class;
        }
    }

    public <E> void invoke(FrameBodyHandler<E> handler, Binary payload, E context)
    {
        handler.handleAttach(this, payload, context);
    }


    public static void register(Decoder decoder)
    {
        AttachConstructor constructor = new AttachConstructor();
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, constructor);
        }
    }

    @Override
    public String toString()
    {
        return "Attach{" +
               "name='" + _name + '\'' +
               ", handle=" + _handle +
               ", role=" + (_role ? "RECEIVER" : "SENDER") +
               ", sndSettleMode=" + _sndSettleMode +
               ", rcvSettleMode=" + _rcvSettleMode +
               ", source=" + _source +
               ", target=" + _target +
               ", unsettled=" + _unsettled +
               ", incompleteUnsettled=" + _incompleteUnsettled +
               ", initialDeliveryCount=" + _initialDeliveryCount +
               ", maxMessageSize=" + _maxMessageSize +
               ", offeredCapabilities=" + (_offeredCapabilities == null ? null : Arrays.asList(_offeredCapabilities)) +
               ", desiredCapabilities=" + (_desiredCapabilities == null ? null : Arrays.asList(_desiredCapabilities)) +
               ", properties=" + _properties +
               '}';
    }
}
