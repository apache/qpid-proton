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

package org.apache.qpid.proton.message2;

import java.util.Date;
import java.util.List;

import org.apache.qpid.proton.codec2.CodecHelper;
import org.apache.qpid.proton.codec2.DecodeException;
import org.apache.qpid.proton.codec2.DescribedTypeFactory;
import org.apache.qpid.proton.codec2.Encodable;
import org.apache.qpid.proton.codec2.Encoder;

public final class Properties implements Encodable
{
    public final static long DESCRIPTOR_LONG = 0x0000000000000073L;

    public final static String DESCRIPTOR_STRING = "amqp:properties:list";

    public final static Factory FACTORY = new Factory();

    private Object _messageId;

    private byte[] _userId;

    private String _to;

    private String _subject;

    private String _replyTo;

    private Object _correlationId;

    private String _contentType;

    private String _contentEncoding;

    private Date _absoluteExpiryTime;

    private Date _creationTime;

    private String _groupId;

    private int _groupSequence;

    private String _replyToGroupId;

    public Object getMessageId()
    {
        return _messageId;
    }

    public void setMessageId(Object messageId)
    {
        _messageId = messageId;
    }

    public byte[] getUserId()
    {
        return _userId;
    }

    public void setUserId(byte[] userId)
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

    public String getContentType()
    {
        return _contentType;
    }

    public void setContentType(String contentType)
    {
        _contentType = contentType;
    }

    public String getContentEncoding()
    {
        return _contentEncoding;
    }

    public void setContentEncoding(String contentEncoding)
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

    public int getGroupSequence()
    {
        return _groupSequence;
    }

    public void setGroupSequence(int groupSequence)
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

    @Override
    public void encode(Encoder encoder)
    {
        encoder.putDescriptor();
        encoder.putUlong(DESCRIPTOR_LONG);
        encoder.putList();
        CodecHelper.encodeObject(encoder, _messageId);
        encoder.putBinary(_userId, 0, _userId.length);
        encoder.putString(_to);
        encoder.putString(_subject);
        encoder.putString(_replyTo);
        CodecHelper.encodeObject(encoder, _correlationId);
        encoder.putSymbol(_contentType);
        encoder.putSymbol(_contentEncoding);
        encoder.putTimestamp(_absoluteExpiryTime.getTime());
        encoder.putTimestamp(_creationTime.getTime());
        encoder.putString(_groupId);
        encoder.putUint(_groupSequence);
        encoder.putString(_replyToGroupId);
        encoder.end();
    }

    public static final class Factory implements DescribedTypeFactory
    {
        @SuppressWarnings("unchecked")
        public Object create(Object in) throws DecodeException
        {
            List<Object> l = (List<Object>) in;
            Properties props = new Properties();

            switch (13 - l.size())
            {

            case 0:
                props.setReplyToGroupId((String) l.get(12));
            case 1:
                props.setGroupSequence((Integer) l.get(11));
            case 2:
                props.setGroupId((String) l.get(10));
            case 3:
                props.setCreationTime(new Date((Long) l.get(9)));
            case 4:
                props.setAbsoluteExpiryTime(new Date((Long) l.get(8)));
            case 5:
                props.setContentEncoding((String) l.get(7));
            case 6:
                props.setContentType((String) l.get(6));
            case 7:
                props.setCorrelationId((Object) l.get(5));
            case 8:
                props.setReplyTo((String) l.get(4));
            case 9:
                props.setSubject((String) l.get(3));
            case 10:
                props.setTo((String) l.get(2));
            case 11:
                props.setUserId((byte[]) l.get(1));
            case 12:
                props.setMessageId((Object) l.get(0));
            }

            return props;
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