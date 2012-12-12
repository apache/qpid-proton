
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
import org.apache.qpid.proton.amqp.UnsignedByte;
import org.apache.qpid.proton.amqp.UnsignedInteger;
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.transport.Attach;
import org.apache.qpid.proton.amqp.transport.ReceiverSettleMode;
import org.apache.qpid.proton.amqp.transport.Role;
import org.apache.qpid.proton.amqp.transport.SenderSettleMode;
import org.apache.qpid.proton.amqp.transport.Source;
import org.apache.qpid.proton.amqp.transport.Target;
import org.apache.qpid.proton.codec.AbstractDescribedType;
import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.codec.EncoderImpl;


public final class AttachType extends AbstractDescribedType<Attach,List> implements DescribedTypeConstructor<Attach>
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000012L), Symbol.valueOf("amqp:attach:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000012L);

    private AttachType(EncoderImpl encoder)
    {
        super(encoder);
    }

    public UnsignedLong getDescriptor()
    {
        return DESCRIPTOR;
    }

    @Override
    protected List wrap(Attach val)
    {
        return new AttachWrapper(val);
    }


    public static class AttachWrapper extends AbstractList
    {

        private Attach _attach;

        public AttachWrapper(Attach attach)
        {
            _attach = attach;
        }

        public Object get(final int index)
            {

                switch(index)
                {
                    case 0:
                        return _attach.getName();
                    case 1:
                        return _attach.getHandle();
                    case 2:
                        return _attach.getRole().getValue();
                    case 3:
                        return _attach.getSndSettleMode().getValue();
                    case 4:
                        return _attach.getRcvSettleMode().getValue();
                    case 5:
                        return _attach.getSource();
                    case 6:
                        return _attach.getTarget();
                    case 7:
                        return _attach.getUnsettled();
                    case 8:
                        return _attach.getIncompleteUnsettled();
                    case 9:
                        return _attach.getInitialDeliveryCount();
                    case 10:
                        return _attach.getMaxMessageSize();
                    case 11:
                        return _attach.getOfferedCapabilities();
                    case 12:
                        return _attach.getDesiredCapabilities();
                    case 13:
                        return _attach.getProperties();
                }

                throw new IllegalStateException("Unknown index " + index);

            }

            public int size()
            {
                return _attach.getProperties() != null
                          ? 14
                          : _attach.getDesiredCapabilities() != null
                          ? 13
                          : _attach.getOfferedCapabilities() != null
                          ? 12
                          : _attach.getMaxMessageSize() != null
                          ? 11
                          : _attach.getInitialDeliveryCount() != null
                          ? 10
                          : _attach.getIncompleteUnsettled()
                          ? 9
                          : _attach.getUnsettled() != null
                          ? 8
                          : _attach.getTarget() != null
                          ? 7
                          : _attach.getSource() != null
                          ? 6
                          : (_attach.getRcvSettleMode() != null && !_attach.getRcvSettleMode().equals(ReceiverSettleMode.FIRST))
                          ? 5
                          : (_attach.getSndSettleMode() != null && !_attach.getSndSettleMode().equals(SenderSettleMode.MIXED))
                          ? 4
                          : 3;

            }

    }

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
                o.setRcvSettleMode(rcvSettleMode == null ? ReceiverSettleMode.FIRST : ReceiverSettleMode.values()[rcvSettleMode.intValue()]);
            case 10:
                UnsignedByte sndSettleMode = (UnsignedByte) l.get(3);
                o.setSndSettleMode(sndSettleMode == null ? SenderSettleMode.MIXED : SenderSettleMode.values()[sndSettleMode.intValue()]);
            case 11:
                o.setRole( Boolean.TRUE.equals( l.get( 2 ) ) ? Role.RECEIVER : Role.SENDER);
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


    public static void register(Decoder decoder, EncoderImpl encoder)
    {
        AttachType type = new AttachType(encoder);
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, type);
        }
        encoder.register(type);
    }
}
