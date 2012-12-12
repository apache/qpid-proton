
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
import org.apache.qpid.proton.amqp.transport.Flow;
import org.apache.qpid.proton.codec.AbstractDescribedType;
import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.codec.EncoderImpl;


public final class FlowType extends AbstractDescribedType<Flow,List> implements DescribedTypeConstructor<Flow>
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000013L), Symbol.valueOf("amqp:flow:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000013L);

    private FlowType(EncoderImpl encoder)
    {
        super(encoder);
    }

    public UnsignedLong getDescriptor()
    {
        return DESCRIPTOR;
    }

    @Override
    protected List wrap(Flow val)
    {
        return new FlowWrapper(val);
    }

    public static class FlowWrapper extends AbstractList
    {


        private Flow _flow;

        public FlowWrapper(Flow flow)
        {
            _flow = flow;
        }

        public Object get(final int index)
        {

            switch(index)
            {
                case 0:
                    return _flow.getNextIncomingId();
                case 1:
                    return _flow.getIncomingWindow();
                case 2:
                    return _flow.getNextOutgoingId();
                case 3:
                    return _flow.getOutgoingWindow();
                case 4:
                    return _flow.getHandle();
                case 5:
                    return _flow.getDeliveryCount();
                case 6:
                    return _flow.getLinkCredit();
                case 7:
                    return _flow.getAvailable();
                case 8:
                    return _flow.getDrain();
                case 9:
                    return _flow.getEcho();
                case 10:
                    return _flow.getProperties();
            }

            throw new IllegalStateException("Unknown index " + index);

        }

        public int size()
        {
            return _flow.getProperties() != null
                      ? 11
                      : _flow.getEcho()
                      ? 10
                      : _flow.getDrain()
                      ? 9
                      : _flow.getAvailable() != null
                      ? 8
                      : _flow.getLinkCredit() != null
                      ? 7
                      : _flow.getDeliveryCount() != null
                      ? 6
                      : _flow.getHandle() != null
                      ? 5
                      : 4;

        }
    }

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


    public static void register(Decoder decoder, EncoderImpl encoder)
    {
        FlowType type = new FlowType(encoder);
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, type);
        }
        encoder.register(type);
    }

}
  