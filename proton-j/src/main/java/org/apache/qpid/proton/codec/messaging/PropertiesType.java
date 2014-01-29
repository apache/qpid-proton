
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


package org.apache.qpid.proton.codec.messaging;

import java.util.AbstractList;
import java.util.Date;
import java.util.List;
import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.UnsignedInteger;
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.messaging.Properties;
import org.apache.qpid.proton.codec.AbstractDescribedType;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.codec.EncoderImpl;


public class PropertiesType  extends AbstractDescribedType<Properties,List> implements DescribedTypeConstructor<Properties>
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000073L), Symbol.valueOf("amqp:properties:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000073L);

    private PropertiesType(EncoderImpl encoder)
    {
        super(encoder);
    }

    public UnsignedLong getDescriptor()
    {
        return DESCRIPTOR;
    }

    @Override
    protected List wrap(Properties val)
    {
        return new PropertiesWrapper(val);
    }

    private static final class PropertiesWrapper extends AbstractList
    {

        private Properties _impl;

        public PropertiesWrapper(Properties propertiesType)
        {
            _impl = propertiesType;
        }

        public Object get(final int index)
        {

            switch(index)
            {
                case 0:
                    return _impl.getMessageId();
                case 1:
                    return _impl.getUserId();
                case 2:
                    return _impl.getTo();
                case 3:
                    return _impl.getSubject();
                case 4:
                    return _impl.getReplyTo();
                case 5:
                    return _impl.getCorrelationId();
                case 6:
                    return _impl.getContentType();
                case 7:
                    return _impl.getContentEncoding();
                case 8:
                    return _impl.getAbsoluteExpiryTime();
                case 9:
                    return _impl.getCreationTime();
                case 10:
                    return _impl.getGroupId();
                case 11:
                    return _impl.getGroupSequence();
                case 12:
                    return _impl.getReplyToGroupId();
            }

            throw new IllegalStateException("Unknown index " + index);

        }

        public int size()
        {
            return _impl.getReplyToGroupId() != null
                      ? 13
                      : _impl.getGroupSequence() != null
                      ? 12
                      : _impl.getGroupId() != null
                      ? 11
                      : _impl.getCreationTime() != null
                      ? 10
                      : _impl.getAbsoluteExpiryTime() != null
                      ? 9
                      : _impl.getContentEncoding() != null
                      ? 8
                      : _impl.getContentType() != null
                      ? 7
                      : _impl.getCorrelationId() != null
                      ? 6
                      : _impl.getReplyTo() != null
                      ? 5
                      : _impl.getSubject() != null
                      ? 4
                      : _impl.getTo() != null
                      ? 3
                      : _impl.getUserId() != null
                      ? 2
                      : _impl.getMessageId() != null
                      ? 1
                      : 0;

        }

    }

        public Properties newInstance(Object described)
        {
            List l = (List) described;

            Properties o = new Properties();


            switch(13 - l.size())
            {

                case 0:
                    o.setReplyToGroupId( (String) l.get( 12 ) );
                case 1:
                    o.setGroupSequence( (UnsignedInteger) l.get( 11 ) );
                case 2:
                    o.setGroupId( (String) l.get( 10 ) );
                case 3:
                    o.setCreationTime( (Date) l.get( 9 ) );
                case 4:
                    o.setAbsoluteExpiryTime( (Date) l.get( 8 ) );
                case 5:
                    o.setContentEncoding( (Symbol) l.get( 7 ) );
                case 6:
                    o.setContentType( (Symbol) l.get( 6 ) );
                case 7:
                    o.setCorrelationId( (Object) l.get( 5 ) );
                case 8:
                    o.setReplyTo( (String) l.get( 4 ) );
                case 9:
                    o.setSubject( (String) l.get( 3 ) );
                case 10:
                    o.setTo( (String) l.get( 2 ) );
                case 11:
                    o.setUserId( (Binary) l.get( 1 ) );
                case 12:
                    o.setMessageId( (Object) l.get( 0 ) );
            }


            return o;
        }

        public Class<Properties> getTypeClass()
        {
            return Properties.class;
        }



    public static void register(Decoder decoder, EncoderImpl encoder)
    {
        PropertiesType type = new PropertiesType(encoder);
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, type);
        }
        encoder.register(type);
    }

}
