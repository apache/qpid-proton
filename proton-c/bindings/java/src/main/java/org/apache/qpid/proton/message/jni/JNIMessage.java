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
package org.apache.qpid.proton.message.jni;

import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.util.Date;
import java.util.List;
import java.util.Map;
import java.util.UUID;

import org.apache.qpid.proton.ProtonCEquivalent;
import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.Decimal32;
import org.apache.qpid.proton.amqp.Decimal64;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.UnsignedByte;
import org.apache.qpid.proton.amqp.UnsignedInteger;
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.UnsignedShort;
import org.apache.qpid.proton.amqp.messaging.AmqpSequence;
import org.apache.qpid.proton.amqp.messaging.AmqpValue;
import org.apache.qpid.proton.amqp.messaging.ApplicationProperties;
import org.apache.qpid.proton.amqp.messaging.Data;
import org.apache.qpid.proton.amqp.messaging.DeliveryAnnotations;
import org.apache.qpid.proton.amqp.messaging.Footer;
import org.apache.qpid.proton.amqp.messaging.Header;
import org.apache.qpid.proton.amqp.messaging.MessageAnnotations;
import org.apache.qpid.proton.amqp.messaging.Properties;
import org.apache.qpid.proton.amqp.messaging.Section;
import org.apache.qpid.proton.codec.jni.JNIData;
import org.apache.qpid.proton.jni.Proton;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_data_t;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_message_t;
import org.apache.qpid.proton.jni.pn_atom_t;
import org.apache.qpid.proton.jni.pn_atom_t_u;
import org.apache.qpid.proton.jni.pn_bytes_t;
import org.apache.qpid.proton.jni.pn_decimal128_t;
import org.apache.qpid.proton.jni.pn_format_t;
import org.apache.qpid.proton.jni.pn_type_t;
import org.apache.qpid.proton.message.Message;
import org.apache.qpid.proton.message.MessageError;
import org.apache.qpid.proton.message.MessageFormat;
import org.apache.qpid.proton.codec.Data.DataType;

public class JNIMessage implements Message
{

    public static final Charset UTF8_CHARSET = Charset.forName("UTF-8");
    public static final Charset ASCII_CHARSET = Charset.forName("US-ASCII");
    private SWIGTYPE_p_pn_message_t _impl;
    private Header _header;
    private DeliveryAnnotations _deliveryAnnotations;
    private MessageAnnotations _messageAnnotations;
    private Properties _properties;
    private ApplicationProperties _applicationProperties;
    private Section _body;
    private Footer _footer;


    JNIMessage()
    {
        _impl = Proton.pn_message();
    }

    public JNIMessage(SWIGTYPE_p_pn_message_t impl)
    {
        _impl = impl;
        postDecode();
    }

    private void postDecode()
    {
        decodeHeader();
        decodeDeliveryAnnotations();
        decodeMessageAnnotations();
        decodeProperties();
        decodeApplicationProperies();
        decodeBody();
        decodeFooter();
    }

    private void decodeHeader()
    {
        _header = new Header();
        _header.setDurable(Proton.pn_message_is_durable(_impl));
        _header.setPriority(UnsignedByte.valueOf((byte) Proton.pn_message_get_priority(_impl)));
        _header.setTtl(UnsignedInteger.valueOf(Proton.pn_message_get_ttl(_impl)));
        _header.setFirstAcquirer(Proton.pn_message_is_first_acquirer(_impl));
        _header.setDeliveryCount(UnsignedInteger.valueOf(Proton.pn_message_get_delivery_count(_impl)));
    }

    private void decodeDeliveryAnnotations()
    {
        JNIData data = new JNIData(Proton.pn_message_instructions(_impl));
        if(data.next() == DataType.MAP)
        {
            Map map = data.getJavaMap();
            _deliveryAnnotations = new DeliveryAnnotations(map);
        }
        else
        {
            _deliveryAnnotations = null;
        }
    }

    private void decodeMessageAnnotations()
    {
        JNIData data = new JNIData(Proton.pn_message_annotations(_impl));
        if(data.next() == DataType.MAP)
        {
            Map map = data.getJavaMap();
            _messageAnnotations = new MessageAnnotations(map);
        }
        else
        {
            _messageAnnotations = null;
        }
    }

    private void decodeProperties()
    {
        _properties = new Properties();
        _properties.setMessageId(convert(Proton.pn_message_get_id(_impl)));
        _properties.setUserId(new Binary(Proton.pn_bytes_to_array(Proton.pn_message_get_user_id(_impl))));
        _properties.setTo(Proton.pn_message_get_address(_impl));
        _properties.setSubject(Proton.pn_message_get_subject(_impl));
        _properties.setReplyTo(Proton.pn_message_get_reply_to(_impl));
        _properties.setCorrelationId(convert(Proton.pn_message_get_correlation_id(_impl)));
        _properties.setContentType(Symbol.valueOf(Proton.pn_message_get_content_type(_impl)));
        _properties.setContentEncoding(Symbol.valueOf(Proton.pn_message_get_content_encoding(_impl)));
        long expiryTime = Proton.pn_message_get_expiry_time(_impl);
        _properties.setAbsoluteExpiryTime(expiryTime == 0l ? null : new Date(expiryTime));
        long creationTime = Proton.pn_message_get_creation_time(_impl);
        _properties.setCreationTime(creationTime == 0l ? null : new Date(creationTime));
        _properties.setGroupId(Proton.pn_message_get_group_id(_impl));
        long groupSequence = getGroupSequence();
        if(groupSequence != 0l || _properties.getGroupId() != null)
        {
            _properties.setGroupSequence(UnsignedInteger.valueOf(groupSequence));
        }
        _properties.setReplyToGroupId(Proton.pn_message_get_reply_to_group_id(_impl));
    }

    private void decodeApplicationProperies()
    {
        JNIData data = new JNIData(Proton.pn_message_properties(_impl));
        if(data.next() == DataType.MAP)
        {
            Map map = data.getJavaMap();
            _applicationProperties = new ApplicationProperties(map);
        }
        else
        {
            _applicationProperties = null;
        }
    }

    private void decodeBody()
    {
        SWIGTYPE_p_pn_data_t body = Proton.pn_message_body(_impl);
        JNIData data = new JNIData(body);
        data.rewind();

        org.apache.qpid.proton.codec.Data.DataType dataType = data.next();
        Section section;
        if(dataType == null)
        {
            section = null;
        }
        else
        {
            Object obj = data.getObject();
            boolean notAmqpValue = Proton.pn_message_is_inferred(_impl);

            if(notAmqpValue && dataType == DataType.BINARY)
            {
                section = new Data((Binary)obj);
            }
            else if(notAmqpValue && dataType == DataType.LIST)
            {
                section = new AmqpSequence((List)obj);
            }
            else
            {
                section = new AmqpValue(obj);
            }
        }
        _body = section;
    }

    private Footer decodeFooter()
    {
        return null;  //TODO
    }

    private void preEncode()
    {
        encodeHeader();
        encodeDeliveryAnnotations();
        encodeMessageAnnotations();
        encodeProperties();
        encodeApplicationProperies();
        encodeBody();
        encodeFooter();
    }

    private void encodeHeader()
    {

        final boolean noHeader = _header == null;

        boolean durable = !noHeader && Boolean.TRUE.equals(_header.getDurable());
        Proton.pn_message_set_durable(_impl, durable);

        byte priority = noHeader || _header.getPriority() == null
                        ? (byte) Proton.PN_DEFAULT_PRIORITY
                        : _header.getPriority().byteValue();
        Proton.pn_message_set_priority(_impl, priority);

        long ttl = noHeader || _header.getTtl() == null ? 0l : _header.getTtl().longValue();
        Proton.pn_message_set_ttl(_impl, ttl);

        boolean firstAcquirer = !noHeader && Boolean.TRUE.equals(_header.getFirstAcquirer());
        Proton.pn_message_set_first_acquirer(_impl, firstAcquirer);

        long deliveryCount = noHeader || _header.getDeliveryCount() == null ? 0L : _header.getDeliveryCount().longValue();
        Proton.pn_message_set_delivery_count(_impl, deliveryCount);
    }

    private void encodeDeliveryAnnotations()
    {
        JNIData data = new JNIData(Proton.pn_message_instructions(_impl));
        data.clear();
        Map annotations;
        if(_deliveryAnnotations != null
           && (annotations = _deliveryAnnotations.getValue()) != null
           && !annotations.isEmpty())
        {
            data.putJavaMap(annotations);
        }
    }

    private void encodeMessageAnnotations()
    {
        JNIData data = new JNIData(Proton.pn_message_annotations(_impl));
        data.clear();
        Map annotations;
        if(_messageAnnotations != null
           && (annotations = _messageAnnotations.getValue()) != null
           && !annotations.isEmpty())
        {
            data.putJavaMap(annotations);
        }
    }

    private void encodeProperties()
    {
        final boolean noProperties = _properties == null;

        Proton.pn_message_set_id(_impl, convertToAtom(noProperties ? null : _properties.getMessageId()));

        pn_bytes_t userId = new pn_bytes_t();;
        if(!noProperties && _properties.getUserId() != null)
        {
            final Binary binary = _properties.getUserId();
            byte[] bytes = new byte[binary.getLength()];
            System.arraycopy(binary.getArray(),binary.getArrayOffset(),bytes,0,binary.getLength());
            Proton.pn_bytes_from_array(userId, bytes);
        }

        Proton.pn_message_set_user_id(_impl, userId);

        Proton.pn_message_set_address(_impl, noProperties ? null : _properties.getTo());
        Proton.pn_message_set_subject(_impl, noProperties ? null : _properties.getSubject());
        Proton.pn_message_set_reply_to(_impl, noProperties ? null : _properties.getReplyTo());
        Proton.pn_message_set_correlation_id(_impl, convertToAtom(noProperties ? null : _properties.getCorrelationId()));
        String contentType = noProperties || _properties.getContentType() == null ? null : _properties.getContentType().toString();
        Proton.pn_message_set_content_type(_impl, contentType);
        String contentEncoding = noProperties || _properties.getContentEncoding() == null
                                 ? null : _properties.getContentEncoding().toString();
        Proton.pn_message_set_content_encoding(_impl, contentEncoding);
        long expiryTime = noProperties || _properties.getAbsoluteExpiryTime() == null ? 0l : _properties.getAbsoluteExpiryTime().getTime();
        Proton.pn_message_set_expiry_time(_impl,expiryTime);
        long creationTime = noProperties || _properties.getCreationTime() == null ? 0l : _properties.getCreationTime().getTime();
        Proton.pn_message_set_creation_time(_impl,creationTime);
        Proton.pn_message_set_group_id(_impl, noProperties ? null : _properties.getGroupId());
        int groupSequence = noProperties || _properties.getGroupSequence() == null ? 0 : _properties.getGroupSequence().intValue();
        Proton.pn_message_set_group_sequence(_impl,groupSequence);
        Proton.pn_message_set_reply_to_group_id(_impl, noProperties ? null : _properties.getReplyToGroupId());

    }

    private void encodeApplicationProperies()
    {
        JNIData data = new JNIData(Proton.pn_message_properties(_impl));
        data.clear();
        Map appProperties;
        if(_applicationProperties != null
           && (appProperties = _applicationProperties.getValue()) != null
           && !appProperties.isEmpty())
        {
            data.putJavaMap(appProperties);
        }
    }

    private void encodeBody()
    {
        JNIData data = new JNIData(Proton.pn_message_body(_impl));
        data.clear();
        if(_body instanceof Data)
        {
            Proton.pn_message_set_inferred(_impl, true);
            data.putBinary(((Data) _body).getValue());
        }
        else if(_body instanceof AmqpSequence)
        {
            Proton.pn_message_set_inferred(_impl, true);
            data.putJavaList(((AmqpSequence)_body).getValue());
        }
        else if(_body instanceof AmqpValue)
        {
            Proton.pn_message_set_inferred(_impl, false);
            data.putObject(((AmqpValue) _body).getValue());
        }
    }

    private void encodeFooter()
    {
        // TODO
    }

    @Override
    @ProtonCEquivalent("pn_message_is_durable")
    public boolean isDurable()
    {
        return (_header == null || _header.getDurable() == null) ? false : _header.getDurable();
    }


    @Override
    @ProtonCEquivalent("pn_message_get_delivery_count")
    public long getDeliveryCount()
    {
        return (_header == null || _header.getDeliveryCount() == null) ? 0l : _header.getDeliveryCount().longValue();
    }


    @Override
    @ProtonCEquivalent("pn_message_get_priority")
    public short getPriority()
    {
        return (_header == null || _header.getPriority() == null)
               ? DEFAULT_PRIORITY
               : _header.getPriority().shortValue();
    }

    @Override
    @ProtonCEquivalent("pn_message_is_first_acquirer")
    public boolean isFirstAcquirer()
    {
        return (_header == null || _header.getFirstAcquirer() == null) ? false : _header.getFirstAcquirer();
    }

    @Override
    @ProtonCEquivalent("pn_message_get_ttl")
    public long getTtl()
    {
        return (_header == null || _header.getTtl() == null) ? 0l : _header.getTtl().longValue();
    }

    @Override
    @ProtonCEquivalent("pn_message_set_durable")
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
    @ProtonCEquivalent("pn_message_set_ttl")
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
    @ProtonCEquivalent("pn_message_set_delivery_count")
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
    @ProtonCEquivalent("pn_message_set_first_acquirer")
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
    @ProtonCEquivalent("pn_message_set_priority")
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
    @ProtonCEquivalent("pn_message_get_id")
    public Object getMessageId()
    {
        return _properties == null ? null : _properties.getMessageId();
    }

    @Override
    @ProtonCEquivalent("pn_message_get_group_sequence")
    public long getGroupSequence()
    {
        return (_properties == null || _properties.getGroupSequence() == null) ? 0l : _properties.getGroupSequence().intValue();
    }

    @Override
    @ProtonCEquivalent("pn_message_get_reply_to_group_id")
    public String getReplyToGroupId()
    {
        return _properties == null ? null : _properties.getReplyToGroupId();
    }

    @Override
    @ProtonCEquivalent("pn_message_get_creation_time")
    public long getCreationTime()
    {
        return (_properties == null || _properties.getCreationTime() == null) ? 0l : _properties.getCreationTime().getTime();
    }

    @Override
    @ProtonCEquivalent("pn_message_get_address")
    public String getAddress()
    {
        return _properties == null ? null : _properties.getTo();
    }

    @Override
    @ProtonCEquivalent("pn_message_get_user_id")
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
    @ProtonCEquivalent("pn_message_get_reply_to")
    public String getReplyTo()
    {
        return _properties == null ? null : _properties.getReplyTo();
    }

    @Override
    @ProtonCEquivalent("pn_message_get_group_id")
    public String getGroupId()
    {
        return _properties == null ? null : _properties.getGroupId();
    }

    @Override
    @ProtonCEquivalent("pn_message_get_content_type")
    public String getContentType()
    {
        return (_properties == null || _properties.getContentType() == null) ? null : _properties.getContentType().toString();
    }

    @Override
    @ProtonCEquivalent("pn_message_get_expiry_time")
    public long getExpiryTime()
    {
        return (_properties == null || _properties.getAbsoluteExpiryTime() == null) ? 0l : _properties.getAbsoluteExpiryTime().getTime();
    }

    @Override
    @ProtonCEquivalent("pn_message_get_correlation_id")
    public Object getCorrelationId()
    {
        return (_properties == null) ? null : _properties.getCorrelationId();
    }

    @Override
    @ProtonCEquivalent("pn_message_get_content_encoding")
    public String getContentEncoding()
    {
        return (_properties == null || _properties.getContentEncoding() == null) ? null : _properties.getContentEncoding().toString();
    }

    @Override
    @ProtonCEquivalent("pn_message_get_subject")
    public String getSubject()
    {
        return _properties == null ? null : _properties.getSubject();
    }


    @Override
    @ProtonCEquivalent("pn_message_set_group_sequence")
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
    @ProtonCEquivalent("pn_message_set_user_id")
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
    @ProtonCEquivalent("pn_message_set_creation_time")
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
    @ProtonCEquivalent("pn_message_set_subject")
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
    @ProtonCEquivalent("pn_message_set_group_id")
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
    @ProtonCEquivalent("pn_message_set_address")
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
    @ProtonCEquivalent("pn_message_set_expiry_time")
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
    @ProtonCEquivalent("pn_message_set_reply_to_group_id")
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
    @ProtonCEquivalent("pn_message_set_content_encoding")
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
    @ProtonCEquivalent("pn_message_set_content_type")
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
    @ProtonCEquivalent("pn_message_set_reply_to")
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
    @ProtonCEquivalent("pn_message_set_correlation_id")
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
    @ProtonCEquivalent("pn_message_set_id")
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


    private pn_atom_t convertToAtom(Object o)
    {
        pn_atom_t atom = new pn_atom_t();

//PN_NULL
        if(o == null)
        {
            atom.setType(pn_type_t.PN_NULL);
        }
//PN_BOOL
        else if(o instanceof Boolean)
        {
            atom.setType(pn_type_t.PN_BOOL);
            atom.getU().setAs_bool((Boolean)o);
        }
//PN_UBYTE
        else if(o instanceof UnsignedByte)
        {
            atom.setType(pn_type_t.PN_UBYTE);
            atom.getU().setAs_ubyte(((UnsignedByte) o).shortValue());
        }
//PN_BYTE
        else if(o instanceof Byte)
        {
            atom.setType(pn_type_t.PN_BYTE);
            atom.getU().setAs_byte((Byte) o);
        }
//PN_USHORT
        else if(o instanceof UnsignedShort)
        {
            atom.setType(pn_type_t.PN_USHORT);
            atom.getU().setAs_ushort(((UnsignedShort) o).intValue());
        }
//PN_SHORT
        else if(o instanceof Short)
        {
            atom.setType(pn_type_t.PN_SHORT);
            atom.getU().setAs_short((Short) o);
        }
//PN_UINT
        else if(o instanceof UnsignedInteger)
        {
            atom.setType(pn_type_t.PN_UINT);
            atom.getU().setAs_uint(((UnsignedInteger) o).longValue());
        }
//PN_INT
        else if(o instanceof Integer)
        {
            atom.setType(pn_type_t.PN_INT);
            atom.getU().setAs_int((Integer) o);
        }
//PN_CHAR
        else if(o instanceof Character)
        {
            atom.setType(pn_type_t.PN_CHAR);
            atom.getU().setAs_char((Character) o);
        }
//PN_ULONG
        else if(o instanceof UnsignedLong)
        {
            atom.setType(pn_type_t.PN_ULONG);
            atom.getU().setAs_ulong(((UnsignedLong) o).bigIntegerValue());
        }
//PN_LONG
        else if(o instanceof Long)
        {
            atom.setType(pn_type_t.PN_LONG);
            atom.getU().setAs_long((Long) o);
        }
//PN_TIMESTAMP
        else if(o instanceof Date)
        {
            atom.setType(pn_type_t.PN_TIMESTAMP);
            atom.getU().setAs_timestamp(((Date)o).getTime());
        }
//PN_FLOAT
        else if(o instanceof Float)
        {
            atom.setType(pn_type_t.PN_FLOAT);
            atom.getU().setAs_float((Float) o);
        }
//PN_DOUBLE
        else if(o instanceof Double)
        {
            atom.setType(pn_type_t.PN_DOUBLE);
            atom.getU().setAs_double((Double) o);
        }
//PN_DECIMAL32
        // TODO
//PN_DECIMAL64
        // TODO
//PN_DECIMAL128
        // TODO
//PN_UUID
        else if (o instanceof UUID)
        {
            byte[] bytes = new byte[16];
            ByteBuffer buf = ByteBuffer.wrap(bytes);
            UUID uuid = (UUID) o;
            buf.putLong(uuid.getMostSignificantBits());
            buf.putLong(uuid.getLeastSignificantBits());
            atom.setType(pn_type_t.PN_UUID);
            pn_bytes_t val = new pn_bytes_t();
            Proton.pn_bytes_from_array(val, bytes);
            atom.getU().setAs_bytes(val);
            val.delete();
        }
//PN_BINARY
        else if(o instanceof byte[] || o instanceof Binary)
        {
            byte[] bytes = (o instanceof byte[]) ? (byte[])o : ((Binary)o).getArray();
            atom.setType(pn_type_t.PN_BINARY);
            pn_bytes_t val = new pn_bytes_t();
            Proton.pn_bytes_from_array(val, bytes);
            atom.getU().setAs_bytes(val);
            val.delete();
        }
//PN_STRING
        else if(o instanceof String)
        {
            byte[] bytes = ((String)o).getBytes(UTF8_CHARSET);
            atom.setType(pn_type_t.PN_STRING);
            pn_bytes_t val = new pn_bytes_t();
            Proton.pn_bytes_from_array(val, bytes);
            atom.getU().setAs_bytes(val);
            val.delete();
        }
//PN_SYMBOL
        else if(o instanceof Symbol)
        {
            byte[] bytes = ((Symbol)o).toString().getBytes(ASCII_CHARSET);
            atom.setType(pn_type_t.PN_STRING);
            pn_bytes_t val = new pn_bytes_t();
            Proton.pn_bytes_from_array(val, bytes);
            atom.getU().setAs_bytes(val);
            val.delete();
        }
//PN_DESCRIBED
        // TODO
//PN_ARRAY
        // TODO
//PN_LIST
        // TODO
//PN_MAP
        // TODO

        return atom;
    }

    private Object convert(pn_atom_t atom)
    {
        if(atom != null)
        {
            pn_type_t type = atom.getType();
            pn_atom_t_u value = atom.getU();

            if(pn_type_t.PN_BINARY.equals(type))
            {
                new Binary(Proton.pn_bytes_to_array(value.getAs_bytes()));
            }
            else if(pn_type_t.PN_STRING.equals(type))
            {
                return new String(Proton.pn_bytes_to_array(value.getAs_bytes()), UTF8_CHARSET);
            }
            else if(pn_type_t.PN_SYMBOL.equals(type))
            {
                return Symbol.valueOf(new String(Proton.pn_bytes_to_array(value.getAs_bytes()), ASCII_CHARSET));
            }
            else if(pn_type_t.PN_ARRAY.equals(type))
            {
               // TODO
            }
            else if(pn_type_t.PN_BOOL.equals(type))
            {
                return value.getAs_bool();
            }
            else if(pn_type_t.PN_BYTE.equals(type))
            {
                return value.getAs_byte();
            }
            else if(pn_type_t.PN_CHAR.equals(type))
            {
                return (char) value.getAs_char();
            }
            else if(pn_type_t.PN_DECIMAL128.equals(type))
            {
                pn_decimal128_t d128 = value.getAs_decimal128();

                // TODO
            }
            else if(pn_type_t.PN_DECIMAL64.equals(type))
            {
                return new Decimal64(value.getAs_decimal64().longValue());
            }
            else if(pn_type_t.PN_DECIMAL32.equals(type))
            {
                return new Decimal32((int)value.getAs_decimal32());
            }
            else if(pn_type_t.PN_DESCRIBED.equals(type))
            {
                // TODO
            }
            else if(pn_type_t.PN_DOUBLE.equals(type))
            {
                return value.getAs_double();
            }
            else if(pn_type_t.PN_FLOAT.equals(type))
            {
                return value.getAs_float();
            }
            else if(pn_type_t.PN_INT.equals(type))
            {
                return value.getAs_int();
            }
            else if(pn_type_t.PN_LIST.equals(type))
            {
                // TODO
            }
            else if(pn_type_t.PN_LONG.equals(type))
            {
                return value.getAs_long();
            }
            else if(pn_type_t.PN_MAP.equals(type))
            {
                // TODO
            }
            else if(pn_type_t.PN_NULL.equals(type))
            {
                return null;
            }
            else if(pn_type_t.PN_SHORT.equals(type))
            {
                return value.getAs_short();
            }
            else if(pn_type_t.PN_TIMESTAMP.equals(type))
            {
                return new Date(value.getAs_timestamp());
            }
            else if(pn_type_t.PN_UBYTE.equals(type))
            {
                return UnsignedByte.valueOf((byte) value.getAs_ubyte());
            }
            else if(pn_type_t.PN_UINT.equals(type))
            {
                return UnsignedInteger.valueOf(value.getAs_uint());
            }
            else if(pn_type_t.PN_ULONG.equals(type))
            {
                return new UnsignedLong(value.getAs_ulong().longValue());
            }
            else if(pn_type_t.PN_USHORT.equals(type))
            {
                return UnsignedShort.valueOf((short) value.getAs_ushort());
            }
            else if(pn_type_t.PN_UUID.equals(type))
            {
                byte[] b = Proton.pn_bytes_to_array(value.getAs_bytes());
                ByteBuffer buf = ByteBuffer.wrap(b);
                return new UUID(buf.getLong(), buf.getLong());
            }

        }
        return null;  //TODO
    }

    @Override
    @ProtonCEquivalent("pn_message_decode")
    public int decode(byte[] data, int offset, int length)
    {
        clear();
        final int bytes = Proton.pn_message_decode(_impl, ByteBuffer.wrap(data, offset, length));
        postDecode();
        return bytes;
    }

    @Override
    @ProtonCEquivalent("pn_message_encode")
    public int encode(byte[] data, int offset, int length)
    {
        try
        {
        preEncode();
        ByteBuffer buffer = ByteBuffer.wrap(data, offset, length);
        int status = Proton.pn_message_encode(_impl, buffer);
        return status == 0 ? buffer.position() : status;
        }
        catch(RuntimeException e)
        {
            e.printStackTrace();
            throw e;
        }
    }

    @Override
    @ProtonCEquivalent("pn_message_set_format")
    public void setMessageFormat(MessageFormat format)
    {
        Proton.pn_message_set_format(_impl, convertMessageFormat(format));
    }

    private pn_format_t convertMessageFormat(MessageFormat format)
    {
        switch(format)
        {
            case AMQP:
                return pn_format_t.PN_AMQP;
            case DATA:
                return pn_format_t.PN_DATA;
            case JSON:
                return pn_format_t.PN_JSON;
            case TEXT:
                return pn_format_t.PN_TEXT;
        }
        // TODO
        return pn_format_t.PN_AMQP;
    }

    @Override
    @ProtonCEquivalent("pn_message_get_format")
    public MessageFormat getMessageFormat()
    {
        return convertMessageFormat(Proton.pn_message_get_format(_impl));
    }

    private MessageFormat convertMessageFormat(pn_format_t format)
    {
        if(format == pn_format_t.PN_AMQP)
        {
            return MessageFormat.AMQP;
        }
        if(format == pn_format_t.PN_DATA)
        {
            return MessageFormat.DATA;
        }
        if(format == pn_format_t.PN_JSON)
        {
            return MessageFormat.JSON;
        }
        if(format == pn_format_t.PN_TEXT)
        {
            return MessageFormat.TEXT;
        }
        // TODO
        return MessageFormat.AMQP;
    }

    @Override
    @ProtonCEquivalent("pn_message_clear")
    public void clear()
    {
        _header = null;
        _deliveryAnnotations = null;
        _messageAnnotations = null;
        _properties = null;
        _applicationProperties = null;
        _body = null;
        _footer = null;
        Proton.pn_message_clear(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_message_error")
    public MessageError getError()
    {
        return null;  //To change body of implemented methods use File | Settings | File Templates.
    }


    @Override
    @ProtonCEquivalent("pn_message_load")
    public void load(Object data)
    {
        byte[] bytes;

        if(data instanceof byte[])
        {
            bytes = (byte[])data;
        }
        else if(data instanceof Binary)
        {
            bytes = ((Binary)data).getArray();
        }
        else if(data instanceof String)
        {

            byte[] temp = ((String)data).getBytes(UTF8_CHARSET);
            if(getMessageFormat() == MessageFormat.DATA)
            {
                bytes = temp;
            }
            else
            {
                // String needs to be null terminated
                bytes = new byte[temp.length+1];
                System.arraycopy(temp,0,bytes,0,temp.length);
            }

        }
        else
        {
            bytes = new byte[getMessageFormat() == MessageFormat.DATA ? 0 : 1];
        }
        int rval = Proton.pn_message_load(_impl, ByteBuffer.wrap(bytes));
        decodeBody();
    }

    @Override
    @ProtonCEquivalent("pn_message_save")
    public Object save()
    {

        int size;
        int sz = 16;
        ByteBuffer buf;
        byte[] data;
        do
        {
            sz = sz*2;
            data = new byte[sz];
            buf = ByteBuffer.wrap(data);
            size = Proton.pn_message_save(_impl, buf);
        }
        while(size == Proton.PN_OVERFLOW);

        byte[] rval = new byte[buf.position()];
        buf.flip();
        buf.get(rval);

        return rval;
    }

    @Override
    public String toAMQPFormat(Object value)
    {
        //TODO
        throw new UnsupportedOperationException();
    }

    @Override
    public Object parseAMQPFormat(String value)
    {
        //TODO
        throw new UnsupportedOperationException();
    }

    @Override
    public void setFooter(Footer footer)
    {
        _footer = footer;
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
    @ProtonCEquivalent("pn_message_annotations")
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
    @ProtonCEquivalent("pn_message_properties")
    public ApplicationProperties getApplicationProperties()
    {
        return _applicationProperties;
    }

    @Override
    @ProtonCEquivalent("pn_message_body")
    public Section getBody()
    {
        return _body;
    }

    public Footer getFooter()
    {
        return _footer;
    }

    public void setHeader(Header header)
    {
        _header = header;
    }

    public void setDeliveryAnnotations(DeliveryAnnotations deliveryAnnotations)
    {
        _deliveryAnnotations = deliveryAnnotations;
    }

    public void setMessageAnnotations(MessageAnnotations messageAnnotations)
    {
        _messageAnnotations = messageAnnotations;
    }

    public void setProperties(Properties properties)
    {
        _properties = properties;
    }

    public void setApplicationProperties(ApplicationProperties applicationProperties)
    {
        _applicationProperties = applicationProperties;
    }

    public void setBody(Section body)
    {


        //TODO
    }


    public SWIGTYPE_p_pn_message_t getImpl()
    {
        preEncode();
        return _impl;
    }
}
