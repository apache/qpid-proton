
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


package org.apache.qpid.proton.type.messaging;
import java.util.Date;
import java.util.List;
import java.util.AbstractList;


import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.type.*;


public class Properties
      implements DescribedType , Section
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000073L), Symbol.valueOf("amqp:properties:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000073L);
    private final PropertiesWrapper _wrapper = new PropertiesWrapper();

    private Object _messageId;
    private Binary _userId;
    private String _to;
    private String _subject;
    private String _replyTo;
    private Object _correlationId;
    private Symbol _contentType;
    private Symbol _contentEncoding;
    private Date _absoluteExpiryTime;
    private Date _creationTime;
    private String _groupId;
    private UnsignedInteger _groupSequence;
    private String _replyToGroupId;

    public Object getMessageId()
    {
        return _messageId;
    }

    public void setMessageId(Object messageId)
    {
        _messageId = messageId;
    }

    public Binary getUserId()
    {
        return _userId;
    }

    public void setUserId(Binary userId)
    {
        _userId = userId;
    }

    public String getTo()
    {
        return _to;
    }

    public void setTo(String to)
    {
        _to = to;
    }

    public String getSubject()
    {
        return _subject;
    }

    public void setSubject(String subject)
    {
        _subject = subject;
    }

    public String getReplyTo()
    {
        return _replyTo;
    }

    public void setReplyTo(String replyTo)
    {
        _replyTo = replyTo;
    }

    public Object getCorrelationId()
    {
        return _correlationId;
    }

    public void setCorrelationId(Object correlationId)
    {
        _correlationId = correlationId;
    }

    public Symbol getContentType()
    {
        return _contentType;
    }

    public void setContentType(Symbol contentType)
    {
        _contentType = contentType;
    }

    public Symbol getContentEncoding()
    {
        return _contentEncoding;
    }

    public void setContentEncoding(Symbol contentEncoding)
    {
        _contentEncoding = contentEncoding;
    }

    public Date getAbsoluteExpiryTime()
    {
        return _absoluteExpiryTime;
    }

    public void setAbsoluteExpiryTime(Date absoluteExpiryTime)
    {
        _absoluteExpiryTime = absoluteExpiryTime;
    }

    public Date getCreationTime()
    {
        return _creationTime;
    }

    public void setCreationTime(Date creationTime)
    {
        _creationTime = creationTime;
    }

    public String getGroupId()
    {
        return _groupId;
    }

    public void setGroupId(String groupId)
    {
        _groupId = groupId;
    }

    public UnsignedInteger getGroupSequence()
    {
        return _groupSequence;
    }

    public void setGroupSequence(UnsignedInteger groupSequence)
    {
        _groupSequence = groupSequence;
    }

    public String getReplyToGroupId()
    {
        return _replyToGroupId;
    }

    public void setReplyToGroupId(String replyToGroupId)
    {
        _replyToGroupId = replyToGroupId;
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
                return _messageId;
            case 1:
                return _userId;
            case 2:
                return _to;
            case 3:
                return _subject;
            case 4:
                return _replyTo;
            case 5:
                return _correlationId;
            case 6:
                return _contentType;
            case 7:
                return _contentEncoding;
            case 8:
                return _absoluteExpiryTime;
            case 9:
                return _creationTime;
            case 10:
                return _groupId;
            case 11:
                return _groupSequence;
            case 12:
                return _replyToGroupId;
        }

        throw new IllegalStateException("Unknown index " + index);

    }

    public int size()
    {
        return _replyToGroupId != null
                  ? 13
                  : _groupSequence != null
                  ? 12
                  : _groupId != null
                  ? 11
                  : _creationTime != null
                  ? 10
                  : _absoluteExpiryTime != null
                  ? 9
                  : _contentEncoding != null
                  ? 8
                  : _contentType != null
                  ? 7
                  : _correlationId != null
                  ? 6
                  : _replyTo != null
                  ? 5
                  : _subject != null
                  ? 4
                  : _to != null
                  ? 3
                  : _userId != null
                  ? 2
                  : _messageId != null
                  ? 1
                  : 0;

    }


    public final class PropertiesWrapper extends AbstractList
    {

        @Override
        public Object get(final int index)
        {
            return Properties.this.get(index);
        }

        @Override
        public int size()
        {
            return Properties.this.size();
        }
    }

    private static class PropertiesConstructor implements DescribedTypeConstructor<Properties>
    {
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
    }


    public static void register(Decoder decoder)
    {
        PropertiesConstructor constructor = new PropertiesConstructor();
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, constructor);
        }
    }

    @Override
    public String toString()
    {
        return "Properties{" +
               "messageId=" + _messageId +
               ", userId=" + _userId +
               ", to='" + _to + '\'' +
               ", subject='" + _subject + '\'' +
               ", replyTo='" + _replyTo + '\'' +
               ", correlationId=" + _correlationId +
               ", contentType=" + _contentType +
               ", contentEncoding=" + _contentEncoding +
               ", absoluteExpiryTime=" + _absoluteExpiryTime +
               ", creationTime=" + _creationTime +
               ", groupId='" + _groupId + '\'' +
               ", groupSequence=" + _groupSequence +
               ", replyToGroupId='" + _replyToGroupId + '\'' +
               '}';
    }
}
