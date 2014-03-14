
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
import org.apache.qpid.proton.amqp.transport.Open;
import org.apache.qpid.proton.codec.AbstractDescribedType;
import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.codec.EncoderImpl;


public final class OpenType extends AbstractDescribedType<Open,List> implements DescribedTypeConstructor<Open>
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000010L), Symbol.valueOf("amqp:open:list"), 
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000010L);

    private OpenType(EncoderImpl encoder)
    {
        super(encoder);
    }


    public UnsignedLong getDescriptor()
    {
        return DESCRIPTOR;
    }

    @Override
    protected List wrap(Open val)
    {
        return new OpenWrapper(val);
    }


    public static class OpenWrapper extends AbstractList
    {

        private Open _open;

        public OpenWrapper(Open open)
        {
            _open = open;
        }

        public Object get(final int index)
        {

            switch(index)
            {
                case 0:
                    return _open.getContainerId();
                case 1:
                    return _open.getHostname();
                case 2:
                    return _open.getMaxFrameSize();
                case 3:
                    return _open.getChannelMax();
                case 4:
                    return _open.getIdleTimeOut();
                case 5:
                    return _open.getOutgoingLocales();
                case 6:
                    return _open.getIncomingLocales();
                case 7:
                    return _open.getOfferedCapabilities();
                case 8:
                    return _open.getDesiredCapabilities();
                case 9:
                    return _open.getProperties();
            }

            throw new IllegalStateException("Unknown index " + index);

        }

        public int size()
        {
            return _open.getProperties() != null
                      ? 10
                      : _open.getDesiredCapabilities() != null
                      ? 9
                      : _open.getOfferedCapabilities() != null
                      ? 8
                      : _open.getIncomingLocales() != null
                      ? 7
                      : _open.getOutgoingLocales() != null
                      ? 6
                      : _open.getIdleTimeOut() != null
                      ? 5
                      : (_open.getChannelMax() != null && !_open.getChannelMax().equals(UnsignedShort.MAX_VALUE))
                      ? 4
                      : (_open.getMaxFrameSize() != null && !_open.getMaxFrameSize().equals(UnsignedInteger.MAX_VALUE))
                      ? 3
                      : _open.getHostname() != null
                      ? 2
                      : 1;

        }

    }

    public Open newInstance(Object described)
    {
        List l = (List) described;

        Open o = new Open();

        if(l.isEmpty())
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


    public static void register(Decoder decoder, EncoderImpl encoder)
    {
        OpenType type = new OpenType(encoder);
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, type);
        }
        encoder.register(type);
    }

}
  