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

import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Map;

import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.codec2.ByteArrayDecoder;
import org.apache.qpid.proton.codec2.ByteArrayEncoder;
import org.apache.qpid.proton.codec2.CodecHelper;
import org.apache.qpid.proton.codec2.POJOBuilder;
import org.apache.qpid.proton.message.MessageError;
import org.apache.qpid.proton.message.MessageFormat;

public class MessageImpl2 implements Message
{
    private final AMQPMessageFormat _parser = new AMQPMessageFormat();

    private Header _header;

    private DeliveryAnnotations _deliveryAnnotations;

    private MessageAnnotations _messageAnnotations;

    private Properties _properties;

    private ApplicationProperties _applicationProperties;

    private Section _body;

    private List<Section> _bodySections;

    private Footer _footer;

    private MessageFormat _format = MessageFormat.DATA;

    private static class EncoderDecoderPair
    {
        ByteArrayEncoder encoder = new ByteArrayEncoder();

        ByteArrayDecoder decoder = new ByteArrayDecoder();
    }

    private static final ThreadLocal<EncoderDecoderPair> tlsCodec = new ThreadLocal<EncoderDecoderPair>()
    {
        @Override
        protected EncoderDecoderPair initialValue()
        {
            return new EncoderDecoderPair();
        }
    };

    // TODO reduce visibility before release.
    public MessageImpl2()
    {
    }

    // TODO reduce visibility before release.
    public MessageImpl2(Header header, DeliveryAnnotations deliveryAnnotations, MessageAnnotations messageAnnotations,
            Properties properties, ApplicationProperties applicationProperties, Section body, Footer footer)
    {
        _header = header;
        _deliveryAnnotations = deliveryAnnotations;
        _messageAnnotations = messageAnnotations;
        _properties = properties;
        _applicationProperties = applicationProperties;
        _body = body;
        _footer = footer;

    }

    @Override
    public boolean isDurable()
    {
        return (_header == null || _header.getDurable() == null) ? false : _header.getDurable();
    }

    @Override
    public long getDeliveryCount()
    {
        return (_header == null ? 0l : _header.getDeliveryCount());
    }

    @Override
    public short getPriority()
    {
        return (_header == null ? DEFAULT_PRIORITY : _header.getPriority());
    }

    @Override
    public boolean isFirstAcquirer()
    {
        return (_header == null || _header.getFirstAcquirer() == null) ? false : _header.getFirstAcquirer();
    }

    @Override
    public long getTtl()
    {
        return (_header == null ? 0l : _header.getTtl());
    }

    @Override
    public void setDurable(boolean durable)
    {
        if (_header == null)
        {
            if (durable)
            {
                _header = new Header();
            }
            else
            {
                return;
            }
        }
        _header.setDurable(durable);
    }

    @Override
    public void setTtl(long ttl)
    {

        if (_header == null)
        {
            if (ttl != 0l)
            {
                _header = new Header();
            }
            else
            {
                return;
            }
        }
        // TODO
        _header.setTtl((int) ttl);
    }

    @Override
    public void setDeliveryCount(long deliveryCount)
    {
        if (_header == null)
        {
            if (deliveryCount == 0l)
            {
                return;
            }
            _header = new Header();
        }
        _header.setDeliveryCount(deliveryCount);
    }

    @Override
    public void setFirstAcquirer(boolean firstAcquirer)
    {

        if (_header == null)
        {
            if (!firstAcquirer)
            {
                return;
            }
            _header = new Header();
        }
        _header.setFirstAcquirer(firstAcquirer);
    }

    @Override
    public void setPriority(short priority)
    {

        if (_header == null)
        {
            if (priority == DEFAULT_PRIORITY)
            {
                return;
            }
            _header = new Header();
        }
        _header.setPriority((byte) priority);
    }

    @Override
    public Object getMessageId()
    {
        return _properties == null ? null : _properties.getMessageId();
    }

    @Override
    public long getGroupSequence()
    {
        return (_properties == null ? 0l : _properties.getGroupSequence());
    }

    @Override
    public String getReplyToGroupId()
    {
        return _properties == null ? null : _properties.getReplyToGroupId();
    }

    @Override
    public long getCreationTime()
    {
        return (_properties == null || _properties.getCreationTime() == null) ? 0l : _properties.getCreationTime()
                .getTime();
    }

    @Override
    public String getAddress()
    {
        return _properties == null ? null : _properties.getTo();
    }

    @Override
    public byte[] getUserId()
    {
        if (_properties == null || _properties.getUserId() == null)
        {
            return null;
        }
        else
        {
            return _properties.getUserId();
        }
    }

    @Override
    public String getReplyTo()
    {
        return _properties == null ? null : _properties.getReplyTo();
    }

    @Override
    public String getGroupId()
    {
        return _properties == null ? null : _properties.getGroupId();
    }

    @Override
    public String getContentType()
    {
        return (_properties == null || _properties.getContentType() == null) ? null : _properties.getContentType();
    }

    @Override
    public long getExpiryTime()
    {
        return (_properties == null || _properties.getAbsoluteExpiryTime() == null) ? 0l : _properties
                .getAbsoluteExpiryTime().getTime();
    }

    @Override
    public Object getCorrelationId()
    {
        return (_properties == null) ? null : _properties.getCorrelationId();
    }

    @Override
    public String getContentEncoding()
    {
        return (_properties == null || _properties.getContentEncoding() == null) ? null : _properties
                .getContentEncoding();
    }

    @Override
    public String getSubject()
    {
        return _properties == null ? null : _properties.getSubject();
    }

    @Override
    public void setGroupSequence(long groupSequence)
    {
        if (_properties == null)
        {
            if (groupSequence == 0l)
            {
                return;
            }
            else
            {
                _properties = new Properties();
            }
        }
        _properties.setGroupSequence((int) groupSequence);
    }

    @Override
    public void setUserId(byte[] userId)
    {
        if (userId == null)
        {
            if (_properties != null)
            {
                _properties.setUserId(null);
            }

        }
        else
        {
            if (_properties == null)
            {
                _properties = new Properties();
            }
            _properties.setUserId(userId);
        }
    }

    @Override
    public void setCreationTime(long creationTime)
    {
        if (_properties == null)
        {
            if (creationTime == 0l)
            {
                return;
            }
            _properties = new Properties();

        }
        _properties.setCreationTime(new Date(creationTime));
    }

    @Override
    public void setSubject(String subject)
    {
        if (_properties == null)
        {
            if (subject == null)
            {
                return;
            }
            _properties = new Properties();
        }
        _properties.setSubject(subject);
    }

    @Override
    public void setGroupId(String groupId)
    {
        if (_properties == null)
        {
            if (groupId == null)
            {
                return;
            }
            _properties = new Properties();
        }
        _properties.setGroupId(groupId);
    }

    @Override
    public void setAddress(String to)
    {
        if (_properties == null)
        {
            if (to == null)
            {
                return;
            }
            _properties = new Properties();
        }
        _properties.setTo(to);
    }

    @Override
    public void setExpiryTime(long absoluteExpiryTime)
    {
        if (_properties == null)
        {
            if (absoluteExpiryTime == 0l)
            {
                return;
            }
            _properties = new Properties();

        }
        _properties.setAbsoluteExpiryTime(new Date(absoluteExpiryTime));
    }

    @Override
    public void setReplyToGroupId(String replyToGroupId)
    {
        if (_properties == null)
        {
            if (replyToGroupId == null)
            {
                return;
            }
            _properties = new Properties();
        }
        _properties.setReplyToGroupId(replyToGroupId);
    }

    @Override
    public void setContentEncoding(String contentEncoding)
    {
        if (_properties == null)
        {
            if (contentEncoding == null)
            {
                return;
            }
            _properties = new Properties();
        }
        _properties.setContentEncoding(contentEncoding);
    }

    @Override
    public void setContentType(String contentType)
    {
        if (_properties == null)
        {
            if (contentType == null)
            {
                return;
            }
            _properties = new Properties();
        }
        _properties.setContentType(contentType);
    }

    @Override
    public void setReplyTo(String replyTo)
    {

        if (_properties == null)
        {
            if (replyTo == null)
            {
                return;
            }
            _properties = new Properties();
        }
        _properties.setReplyTo(replyTo);
    }

    @Override
    public void setCorrelationId(Object correlationId)
    {

        if (_properties == null)
        {
            if (correlationId == null)
            {
                return;
            }
            _properties = new Properties();
        }
        _properties.setCorrelationId(correlationId);
    }

    @Override
    public void setMessageId(Object messageId)
    {

        if (_properties == null)
        {
            if (messageId == null)
            {
                return;
            }
            _properties = new Properties();
        }
        _properties.setMessageId(messageId);
    }

    @Override
    public Header getHeader()
    {
        return _header;
    }

    @Override
    public Map<String, Object> getDeliveryAnnotations()
    {
        if (_deliveryAnnotations != null)
        {
            return _deliveryAnnotations.getValue();
        }
        else
        {
            return null;
        }
    }

    @Override
    public Map<String, Object> getMessageAnnotations()
    {
        if (_messageAnnotations != null)
        {
            return _messageAnnotations.getValue();
        }
        else
        {
            return null;
        }
    }

    @Override
    public Properties getProperties()
    {
        return _properties;
    }

    @Override
    public Map<Object, Object> getApplicationProperties()
    {
        if (_applicationProperties != null)
        {
            return _applicationProperties.getValue();
        }
        else
        {
            return null;
        }
    }

    @Override
    public Object getBody()
    {
        if (_bodySections.size() == 1)
        {
            return _body;
        }
        else if (_bodySections.size() > 1)
        {
            // TODO figure out the best way to provide a composite view.
            // For now just return the list
            return _bodySections;
        }
        else
        {
            return null;
        }
    }

    @Override
    public Footer getFooter()
    {
        return _footer;
    }

    @Override
    public void setHeader(Header header)
    {
        _header = header;
    }

    @Override
    public void setDeliveryAnnotations(Map<String, Object> deliveryAnnotations)
    {
        _deliveryAnnotations = new DeliveryAnnotations(deliveryAnnotations);
    }

    @Override
    public void setMessageAnnotations(Map<String, Object> messageAnnotations)
    {
        _messageAnnotations = new MessageAnnotations(messageAnnotations);
    }

    @Override
    public void setProperties(Properties properties)
    {
        _properties = properties;
    }

    @Override
    public void setApplicationProperties(Map<Object, Object> props)
    {
        _applicationProperties = new ApplicationProperties(props);
    }

    @Override
    public void setBody(Object body)
    {
        if (body instanceof Section)
        {
            _body = (Section) body;
        }
        else
        {
            _body = new AmqpValue(body);
        }
    }

    @Override
    public void setFooter(Footer footer)
    {
        _footer = footer;
    }

    @Override
    public int decode(byte[] data, int offset, int length)
    {
        ByteArrayDecoder decoder = tlsCodec.get().decoder;
        decoder.init(data, 0, data.length);

        _header = null;
        _deliveryAnnotations = null;
        _messageAnnotations = null;
        _properties = null;
        _applicationProperties = null;
        _body = null;
        _footer = null;

        POJOBuilder pb = new POJOBuilder();
        decoder.decode(pb);
        List<Object> msgParts = (List<Object>) pb.build();
        for (Object part : msgParts)
        {
            if (part instanceof Header)
            {
                _header = (Header) part;
            }
            else if (part instanceof DeliveryAnnotations)
            {
                _deliveryAnnotations = (DeliveryAnnotations) part;
            }
            else if (part instanceof MessageAnnotations)
            {
                _messageAnnotations = (MessageAnnotations) part;
            }
            else if (part instanceof Properties)
            {
                _properties = (Properties) part;
            }
            else if (part instanceof ApplicationProperties)
            {
                _applicationProperties = (ApplicationProperties) part;
            }
            else if (part instanceof AmqpValue)
            {
                _body = ((AmqpValue) part);
            }
            else if (part instanceof AmqpSequence || part instanceof Data)
            {
                if (_body == null)
                {
                    _body = ((Section) part);
                }
                else if (_bodySections != null)
                {
                    _bodySections.add((Section) part);
                }
                else
                {
                    _bodySections = new ArrayList<Section>();
                    _bodySections.add(_body);
                    _bodySections.add((Section) part);
                }
            }
            else if (part instanceof Footer)
            {
                _footer = (Footer) part;
            }
        }
        return decoder.getSize();
    }

    // @Override
    public int encode(byte[] data, int offset, int length)
    {
        ByteArrayEncoder encoder = tlsCodec.get().encoder;
        encoder.init(data, 0, data.length);

        if (_header != null)
        {
            _header.encode(encoder);
        }
        if (_deliveryAnnotations != null)
        {
            _deliveryAnnotations.encode(encoder);
        }
        if (_messageAnnotations != null)
        {
            _messageAnnotations.encode(encoder);
        }
        if (_properties != null)
        {
            _properties.encode(encoder);
        }
        if (_applicationProperties != null)
        {
            _applicationProperties.encode(encoder);
        }
        if (_bodySections != null)
        {
            for (Section section : _bodySections)
            {
                CodecHelper.encodeObject(encoder, section);
            }
        }
        else if (_body != null)
        {
            CodecHelper.encodeObject(encoder, _body);
        }
        if (_footer != null)
        {
            _footer.encode(encoder);
        }
        
        byte[] temp = new byte[encoder.getPosition()];
        System.arraycopy(data, offset, temp, 0, encoder.getPosition());
        
        try
        {
            decode(temp, 0, encoder.getPosition());
        }
        catch (Exception e)
        {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
        
        return encoder.getPosition();
    }

    @Override
    public void load(Object data)
    {
        switch (_format)
        {
        case DATA:
            Binary binData;
            if (data instanceof byte[])
            {
                binData = new Binary((byte[]) data);
            }
            else if (data instanceof Binary)
            {
                binData = (Binary) data;
            }
            else if (data instanceof String)
            {
                final String strData = (String) data;
                byte[] bin = new byte[strData.length()];
                for (int i = 0; i < bin.length; i++)
                {
                    bin[i] = (byte) strData.charAt(i);
                }
                binData = new Binary(bin);
            }
            else
            {
                binData = null;
            }
            // _body = new Data(binData);
            break;
        case TEXT:
            _body = new AmqpValue(data == null ? "" : data.toString());
            break;
        default:
            // AMQP
            _body = new AmqpValue(parseAMQPFormat((String) data));
        }

    }

    @Override
    public Object save()
    {
        switch (_format)
        {
        case DATA:
            if (_body instanceof Data)
            {
                return null; // ((Data)_body).getValue().getArray();
            }
            else
                return null;
        case AMQP:
            if (_body instanceof AmqpValue)
            {
                return toAMQPFormat(((AmqpValue) _body).getValue());
            }
            else
            {
                return null;
            }
        case TEXT:
            if (_body instanceof AmqpValue)
            {
                final Object value = ((AmqpValue) _body).getValue();
                return value == null ? "" : value.toString();
            }
            return null;
        default:
            return null;
        }
    }

    @Override
    public String toAMQPFormat(Object value)
    {
        return _parser.encode(value);
    }

    @Override
    public Object parseAMQPFormat(String value)
    {

        Object obj = _parser.format(value);
        return obj;
    }

    @Override
    public void setMessageFormat(MessageFormat format)
    {
        _format = format;
    }

    @Override
    public MessageFormat getMessageFormat()
    {
        return _format;
    }

    @Override
    public void clear()
    {
        _body = null;
        _bodySections.clear();
    }

    @Override
    public MessageError getError()
    {
        return MessageError.OK;
    }

    @Override
    public Section getBodyAsSection(int i)
    {
        if (_bodySections == null)
        {
            return _body;
        }
        else
        {
            // Let the list impl throw ArrayIndexOutofBounds
            return _bodySections.get(i);
        }
    }

    @Override
    public void addBodySection(Section section)
    {
        if (_bodySections == null)
        {
            _bodySections = new ArrayList<Section>();
        }
        _bodySections.add(section);
    }
}