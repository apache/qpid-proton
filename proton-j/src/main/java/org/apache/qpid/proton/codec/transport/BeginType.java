
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
import java.util.Map;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.UnsignedInteger;
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.UnsignedShort;
import org.apache.qpid.proton.amqp.transport.Begin;
import org.apache.qpid.proton.codec.AbstractDescribedType;
import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.codec.EncoderImpl;


public final class BeginType extends AbstractDescribedType<Begin,List> implements DescribedTypeConstructor<Begin>
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000011L), Symbol.valueOf("amqp:begin:list"), 
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000011L);

    private BeginType(EncoderImpl encoder)
    {
        super(encoder);
    }

    public UnsignedLong getDescriptor()
    {
        return DESCRIPTOR;
    }

    @Override
    protected List wrap(Begin val)
    {
        return new BeginWrapper(val);
    }

    private static class BeginWrapper extends AbstractList
    {

        private Begin _begin;

        public BeginWrapper(Begin begin)
        {
            _begin = begin;
        }

        public Object get(final int index)
        {

            switch(index)
            {
                case 0:
                    return _begin.getRemoteChannel();
                case 1:
                    return _begin.getNextOutgoingId();
                case 2:
                    return _begin.getIncomingWindow();
                case 3:
                    return _begin.getOutgoingWindow();
                case 4:
                    return _begin.getHandleMax();
                case 5:
                    return _begin.getOfferedCapabilities();
                case 6:
                    return _begin.getDesiredCapabilities();
                case 7:
                    return _begin.getProperties();
            }

            throw new IllegalStateException("Unknown index " + index);

        }

        public int size()
        {
            return _begin.getProperties() != null
                      ? 8
                      : _begin.getDesiredCapabilities() != null
                      ? 7
                      : _begin.getOfferedCapabilities() != null
                      ? 6
                      : (_begin.getHandleMax() != null && !_begin.getHandleMax().equals(UnsignedInteger.MAX_VALUE))
                      ? 5
                      : 4;

        }
    }

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


    public static void register(Decoder decoder, EncoderImpl encoder)
    {
        BeginType type = new BeginType(encoder);
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, type);
        }
        encoder.register(type);
    }

}
  