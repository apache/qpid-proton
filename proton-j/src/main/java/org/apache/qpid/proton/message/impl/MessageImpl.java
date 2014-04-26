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

package org.apache.qpid.proton.message.impl;

import java.nio.ByteBuffer;
import java.util.Date;
import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.UnsignedByte;
import org.apache.qpid.proton.amqp.UnsignedInteger;
import org.apache.qpid.proton.amqp.messaging.*;
import org.apache.qpid.proton.amqp.messaging.Data;
import org.apache.qpid.proton.codec.*;
import org.apache.qpid.proton.message.*;

public class MessageImpl implements ProtonJMessage
{
    private final AMQPMessageFormat _parser = new AMQPMessageFormat();

    private Header _header;
    private DeliveryAnnotations _deliveryAnnotations;
    private MessageAnnotations _messageAnnotations;
    private Properties _properties;
    private ApplicationProperties _applicationProperties;
    private Section _body;
    private Footer _footer;
    private MessageFormat _format = MessageFormat.DATA;
    
    private static class EncoderDecoderPair {
      DecoderImpl decoder = new DecoderImpl();
      EncoderImpl encoder = new EncoderImpl(decoder);
      {
          AMQPDefinedTypes.registerAllTypes(decoder, encoder);
      }
    }

    private static final ThreadLocal<EncoderDecoderPair> tlsCodec = new ThreadLocal<EncoderDecoderPair>() {
          @Override protected EncoderDecoderPair initialValue() {
            return new EncoderDecoderPair();
          }
      };

    /**
     * @deprecated This constructor's visibility will be reduced to the default scope in a future release.
     * Client code outside this module should use a {@link MessageFactory} instead
     */
    @Deprecated public MessageImpl()
    {
    }

    /**
     * @deprecated This constructor's visibility will be reduced to the default scope in a future release.
     * Client code outside this module should use a {@link MessageFactory} instead
     */
    @Deprecated public MessageImpl(Header header, DeliveryAnnotations deliveryAnnotations, MessageAnnotations messageAnnotations,
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
        return (_header == null || _header.getDeliveryCount() == null) ? 0l : _header.getDeliveryCount().longValue();
    }


    @Override
    public short getPriority()
    {
        return (_header == null || _header.getPriority() == null)
                       ? DEFAULT_PRIORITY
                       : _header.getPriority().shortValue();
    }

    @Override
    public boolean isFirstAcquirer()
    {
        return (_header == null || _header.getFirstAcquirer() == null) ? false : _header.getFirstAcquirer();
    }

    @Override
    public long getTtl()
    {
        return (_header == null || _header.getTtl() == null) ? 0l : _header.getTtl().longValue();
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
        _header.setTtl(UnsignedInteger.valueOf(ttl));
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
        _header.setDeliveryCount(UnsignedInteger.valueOf(deliveryCount));
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
        _header.setPriority(UnsignedByte.valueOf((byte) priority));
    }

    @Override
    public Object getMessageId()
    {
        return _properties == null ? null : _properties.getMessageId();
    }

    @Override
    public long getGroupSequence()
    {
        return (_properties == null || _properties.getGroupSequence() == null) ? 0l : _properties.getGroupSequence().intValue();
    }

    @Override
    public String getReplyToGroupId()
    {
        return _properties == null ? null : _properties.getReplyToGroupId();
    }

    @Override
    public long getCreationTime()
    {
        return (_properties == null || _properties.getCreationTime() == null) ? 0l : _properties.getCreationTime().getTime();
    }

    @Override
    public String getAddress()
    {
        return _properties == null ? null : _properties.getTo();
    }

    @Override
    public byte[] getUserId()
    {
        if(_properties == null || _properties.getUserId() == null)
        {
            return null;
        }
        else
        {
            final Binary userId = _properties.getUserId();
            byte[] id = new byte[userId.getLength()];
            System.arraycopy(userId.getArray(),userId.getArrayOffset(),id,0,userId.getLength());
            return id;
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
        return (_properties == null || _properties.getContentType() == null) ? null : _properties.getContentType().toString();
    }

    @Override
    public long getExpiryTime()
    {
        return (_properties == null || _properties.getAbsoluteExpiryTime() == null) ? 0l : _properties.getAbsoluteExpiryTime().getTime();
    }

    @Override
    public Object getCorrelationId()
    {
        return (_properties == null) ? null : _properties.getCorrelationId();
    }

    @Override
    public String getContentEncoding()
    {
        return (_properties == null || _properties.getContentEncoding() == null) ? null : _properties.getContentEncoding().toString();
    }

    @Override
    public String getSubject()
    {
        return _properties == null ? null : _properties.getSubject();
    }

    @Override
    public void setGroupSequence(long groupSequence)
    {
        if(_properties == null)
        {
            if(groupSequence == 0l)
            {
                return;
            }
            else
            {
                _properties = new Properties();
            }
        }
        _properties.setGroupSequence(UnsignedInteger.valueOf((int) groupSequence));
    }

    @Override
    public void setUserId(byte[] userId)
    {
        if(userId == null)
        {
            if(_properties != null)
            {
                _properties.setUserId(null);
            }

        }
        else
        {
            if(_properties == null)
            {
                _properties = new Properties();
            }
            byte[] id = new byte[userId.length];
            System.arraycopy(userId, 0, id,0, userId.length);
            _properties.setUserId(new Binary(id));
        }
    }

    @Override
    public void setCreationTime(long creationTime)
    {
        if(_properties == null)
        {
            if(creationTime == 0l)
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
        if(_properties == null)
        {
            if(subject == null)
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
        if(_properties == null)
        {
            if(groupId == null)
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
        if(_properties == null)
        {
            if(to == null)
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
        if(_properties == null)
        {
            if(absoluteExpiryTime == 0l)
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
        if(_properties == null)
        {
            if(replyToGroupId == null)
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
        if(_properties == null)
        {
            if(contentEncoding == null)
            {
                return;
            }
            _properties = new Properties();
        }
        _properties.setContentEncoding(Symbol.valueOf(contentEncoding));
    }

    @Override
    public void setContentType(String contentType)
    {
        if(_properties == null)
        {
            if(contentType == null)
            {
                return;
            }
            _properties = new Properties();
        }
        _properties.setContentType(Symbol.valueOf(contentType));
    }

    @Override
    public void setReplyTo(String replyTo)
    {

        if(_properties == null)
        {
            if(replyTo == null)
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

        if(_properties == null)
        {
            if(correlationId == null)
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

        if(_properties == null)
        {
            if(messageId == null)
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
    public DeliveryAnnotations getDeliveryAnnotations()
    {
        return _deliveryAnnotations;
    }

    @Override
    public MessageAnnotations getMessageAnnotations()
    {
        return _messageAnnotations;
    }

    @Override
    public Properties getProperties()
    {
        return _properties;
    }

    @Override
    public ApplicationProperties getApplicationProperties()
    {
        return _applicationProperties;
    }

    @Override
    public Section getBody()
    {
        return _body;
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
    public void setDeliveryAnnotations(DeliveryAnnotations deliveryAnnotations)
    {
        _deliveryAnnotations = deliveryAnnotations;
    }

    @Override
    public void setMessageAnnotations(MessageAnnotations messageAnnotations)
    {
        _messageAnnotations = messageAnnotations;
    }

    @Override
    public void setProperties(Properties properties)
    {
        _properties = properties;
    }

    @Override
    public void setApplicationProperties(ApplicationProperties applicationProperties)
    {
        _applicationProperties = applicationProperties;
    }

    @Override
    public void setBody(Section body)
    {
        _body = body;
    }

    @Override
    public void setFooter(Footer footer)
    {
        _footer = footer;
    }

    @Override
    public int decode(byte[] data, int offset, int length)
    {
        DecoderImpl decoder = tlsCodec.get().decoder;
        final ByteBuffer buffer = ByteBuffer.wrap(data, offset, length);
        decoder.setByteBuffer(buffer);

        _header = null;
        _deliveryAnnotations = null;
        _messageAnnotations = null;
        _properties = null;
        _applicationProperties = null;
        _body = null;
        _footer = null;
        Section section = null;

        if(buffer.hasRemaining())
        {
            section = (Section) decoder.readObject();
        }
        if(section instanceof Header)
        {
            _header = (Header) section;
            if(buffer.hasRemaining())
            {
                section = (Section) decoder.readObject();
            }
            else
            {
                section = null;
            }

        }
        if(section instanceof DeliveryAnnotations)
        {
            _deliveryAnnotations = (DeliveryAnnotations) section;

            if(buffer.hasRemaining())
            {
                section = (Section) decoder.readObject();
            }
            else
            {
                section = null;
            }

        }
        if(section instanceof MessageAnnotations)
        {
            _messageAnnotations = (MessageAnnotations) section;

            if(buffer.hasRemaining())
            {
                section = (Section) decoder.readObject();
            }
            else
            {
                section = null;
            }

        }
        if(section instanceof Properties)
        {
            _properties = (Properties) section;

            if(buffer.hasRemaining())
            {
                section = (Section) decoder.readObject();
            }
            else
            {
                section = null;
            }

        }
        if(section instanceof ApplicationProperties)
        {
            _applicationProperties = (ApplicationProperties) section;

            if(buffer.hasRemaining())
            {
                section = (Section) decoder.readObject();
            }
            else
            {
                section = null;
            }

        }
        if(section != null && !(section instanceof Footer))
        {
            _body = section;

            if(buffer.hasRemaining())
            {
                section = (Section) decoder.readObject();
            }
            else
            {
                section = null;
            }

        }
        if(section instanceof Footer)
        {
            _footer = (Footer) section;

        }

        decoder.setByteBuffer(null);
        
        return length-buffer.remaining();

    }

    @Override
    public int encode(byte[] data, int offset, int length)
    {
        ByteBuffer buffer = ByteBuffer.wrap(data, offset, length);
        return encode(new WritableBuffer.ByteBufferWrapper(buffer));
    }

    @Override
    public int encode2(byte[] data, int offset, int length)
    {
        ByteBuffer buffer = ByteBuffer.wrap(data, offset, length);
        WritableBuffer.ByteBufferWrapper first = new WritableBuffer.ByteBufferWrapper(buffer);
        DroppingWritableBuffer second = new DroppingWritableBuffer();
        CompositeWritableBuffer composite = new CompositeWritableBuffer(first, second);
        int start = composite.position();
        encode(composite);
        return composite.position() - start;
    }

    @Override
    public int encode(WritableBuffer buffer)
    {
        int length = buffer.remaining();
        EncoderImpl encoder = tlsCodec.get().encoder;
        encoder.setByteBuffer(buffer);

        if(getHeader() != null)
        {
            encoder.writeObject(getHeader());
        }
        if(getDeliveryAnnotations() != null)
        {
            encoder.writeObject(getDeliveryAnnotations());
        }
        if(getMessageAnnotations() != null)
        {
            encoder.writeObject(getMessageAnnotations());
        }
        if(getProperties() != null)
        {
            encoder.writeObject(getProperties());
        }
        if(getApplicationProperties() != null)
        {
            encoder.writeObject(getApplicationProperties());
        }
        if(getBody() != null)
        {
            encoder.writeObject(getBody());
        }
        if(getFooter() != null)
        {
            encoder.writeObject(getFooter());
        }
        encoder.setByteBuffer((WritableBuffer)null);

        return length - buffer.remaining();
    }

    @Override
    public void load(Object data)
    {
        switch (_format)
        {
            case DATA:
                Binary binData;
                if(data instanceof byte[])
                {
                    binData = new Binary((byte[])data);
                }
                else if(data instanceof Binary)
                {
                    binData = (Binary) data;
                }
                else if(data instanceof String)
                {
                    final String strData = (String) data;
                    byte[] bin = new byte[strData.length()];
                    for(int i = 0; i < bin.length; i++)
                    {
                        bin[i] = (byte) strData.charAt(i);
                    }
                    binData = new Binary(bin);
                }
                else
                {
                    binData = null;
                }
                _body = new Data(binData);
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
                if(_body instanceof Data)
                {
                    return ((Data)_body).getValue().getArray();
                }
                else return null;
            case AMQP:
                if(_body instanceof AmqpValue)
                {
                    return toAMQPFormat(((AmqpValue) _body).getValue());
                }
                else
                {
                    return null;
                }
            case TEXT:
                if(_body instanceof AmqpValue)
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
    }

    @Override
    public MessageError getError()
    {
        return MessageError.OK;
    }

}
