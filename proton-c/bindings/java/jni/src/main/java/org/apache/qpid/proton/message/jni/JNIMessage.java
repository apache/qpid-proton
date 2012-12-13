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

import java.io.DataInput;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.util.Date;
import java.util.UUID;
import org.apache.qpid.proton.jni.Proton;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_data_t;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_message_t;
import org.apache.qpid.proton.jni.pn_atom_t;
import org.apache.qpid.proton.jni.pn_atom_t_u;
import org.apache.qpid.proton.jni.pn_bytes_t;
import org.apache.qpid.proton.jni.pn_format_t;
import org.apache.qpid.proton.jni.pn_type_t;
import org.apache.qpid.proton.message.Message;
import org.apache.qpid.proton.message.MessageError;
import org.apache.qpid.proton.message.MessageFormat;
import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.Decimal128;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.UnsignedByte;
import org.apache.qpid.proton.amqp.UnsignedInteger;
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.UnsignedShort;
import org.apache.qpid.proton.amqp.messaging.ApplicationProperties;
import org.apache.qpid.proton.amqp.messaging.DeliveryAnnotations;
import org.apache.qpid.proton.amqp.messaging.Header;
import org.apache.qpid.proton.amqp.messaging.Footer;
import org.apache.qpid.proton.amqp.messaging.MessageAnnotations;
import org.apache.qpid.proton.amqp.messaging.Properties;
import org.apache.qpid.proton.amqp.messaging.Section;

public class JNIMessage implements Message
{

    public static final Charset UTF8_CHARSET = Charset.forName("UTF-8");
    public static final Charset ASCII_CHARSET = Charset.forName("US-ASCII");
    private SWIGTYPE_p_pn_message_t _impl;

    JNIMessage()
    {
        _impl = Proton.pn_message();
    }


    @Override
    public boolean isDurable()
    {
        return Proton.pn_message_is_durable(_impl);
    }

    @Override
    public long getDeliveryCount()
    {
        return Proton.pn_message_get_delivery_count(_impl);
    }

    @Override
    public short getPriority()
    {
        return Proton.pn_message_get_priority(_impl);
    }

    @Override
    public boolean isFirstAcquirer()
    {
        return Proton.pn_message_is_first_acquirer(_impl);
    }

    @Override
    public long getTtl()
    {
        return Proton.pn_message_get_ttl(_impl);
    }

    @Override
    public void setDurable(boolean durable)
    {
        Proton.pn_message_set_durable(_impl, durable);
    }

    @Override
    public void setTtl(long ttl)
    {
        Proton.pn_message_set_ttl(_impl, ttl);
    }

    @Override
    public void setDeliveryCount(long deliveryCount)
    {
        Proton.pn_message_set_delivery_count(_impl, deliveryCount);
    }

    @Override
    public void setFirstAcquirer(boolean firstAcquirer)
    {
        Proton.pn_message_set_first_acquirer(_impl, firstAcquirer);
    }

    @Override
    public void setPriority(short priority)
    {
        Proton.pn_message_set_priority(_impl, priority);
    }

    @Override
    public Object getMessageId()
    {
        return convert(Proton.pn_message_get_id(_impl));
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
            pn_bytes_t val = Proton.pn_bytes(bytes.length, bytes);
            atom.getU().setAs_bytes(val);
            val.delete();
        }
//PN_BINARY
        else if(o instanceof byte[] || o instanceof Binary)
        {
            byte[] bytes = (o instanceof byte[]) ? (byte[])o : ((Binary)o).getArray();
            atom.setType(pn_type_t.PN_BINARY);
            pn_bytes_t val = Proton.pn_bytes(bytes.length, bytes);
            atom.getU().setAs_bytes(val);
            val.delete();
        }
//PN_STRING
        else if(o instanceof String)
        {
            byte[] bytes = ((String)o).getBytes(UTF8_CHARSET);
            atom.setType(pn_type_t.PN_STRING);
            pn_bytes_t val = Proton.pn_bytes(bytes.length, bytes);
            atom.getU().setAs_bytes(val);
            val.delete();
        }
//PN_SYMBOL
        else if(o instanceof Symbol)
        {
            byte[] bytes = ((Symbol)o).toString().getBytes(ASCII_CHARSET);
            atom.setType(pn_type_t.PN_STRING);
            pn_bytes_t val = Proton.pn_bytes(bytes.length, bytes);
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
                // TODO
            }
            else if(pn_type_t.PN_DECIMAL64.equals(type))
            {
                // TODO
            }
            else if(pn_type_t.PN_DECIMAL32.equals(type))
            {
                // TODO
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
    public long getGroupSequence()
    {
        return Proton.pn_message_get_group_sequence(_impl);
    }

    @Override
    public String getReplyToGroupId()
    {
        return Proton.pn_message_get_reply_to_group_id(_impl);
    }

    @Override
    public long getCreationTime()
    {
        return Proton.pn_message_get_creation_time(_impl);
    }

    @Override
    public String getAddress()
    {
        return Proton.pn_message_get_address(_impl);
    }

    @Override
    public byte[] getUserId()
    {

        return Proton.pn_bytes_to_array(Proton.pn_message_get_user_id(_impl));

    }

    @Override
    public String getReplyTo()
    {
        return Proton.pn_message_get_reply_to(_impl);
    }

    @Override
    public String getGroupId()
    {
        return Proton.pn_message_get_group_id(_impl);
    }

    @Override
    public String getContentType()
    {
        return Proton.pn_message_get_content_type(_impl);
    }

    @Override
    public long getExpiryTime()
    {
        return Proton.pn_message_get_expiry_time(_impl);
    }

    @Override
    public Object getCorrelationId()
    {
        return convert(Proton.pn_message_get_correlation_id(_impl));
    }

    @Override
    public String getContentEncoding()
    {
        return Proton.pn_message_get_content_encoding(_impl);
    }

    @Override
    public String getSubject()
    {
        return Proton.pn_message_get_subject(_impl);
    }

    @Override
    public void setGroupSequence(long groupSequence)
    {
        Proton.pn_message_set_group_sequence(_impl, (int) groupSequence);
    }

    @Override
    public void setUserId(byte[] userId)
    {
        pn_bytes_t val = Proton.pn_bytes(userId.length, userId);

        Proton.pn_message_set_user_id(_impl, val);

        val.delete();
    }

    @Override
    public void setCreationTime(long creationTime)
    {
        Proton.pn_message_set_creation_time(_impl, creationTime);
    }

    @Override
    public void setSubject(String subject)
    {
        Proton.pn_message_set_subject(_impl, subject);
    }

    @Override
    public void setGroupId(String groupId)
    {
        Proton.pn_message_set_group_id(_impl, groupId);
    }

    @Override
    public void setAddress(String to)
    {
        Proton.pn_message_set_address(_impl, to);
    }

    @Override
    public void setExpiryTime(long absoluteExpiryTime)
    {
        Proton.pn_message_set_expiry_time(_impl,absoluteExpiryTime);
    }

    @Override
    public void setReplyToGroupId(String replyToGroupId)
    {
        Proton.pn_message_set_reply_to_group_id(_impl, replyToGroupId);
    }

    @Override
    public void setContentEncoding(String contentEncoding)
    {
        Proton.pn_message_set_content_encoding(_impl, contentEncoding);
    }

    @Override
    public void setContentType(String contentType)
    {
        Proton.pn_message_set_content_type(_impl, contentType);
    }

    @Override
    public void setReplyTo(String replyTo)
    {
        Proton.pn_message_set_reply_to(_impl, replyTo);
    }

    @Override
    public void setCorrelationId(Object correlationId)
    {
        Proton.pn_message_set_correlation_id(_impl, convertToAtom(correlationId));
    }

    @Override
    public void setMessageId(Object messageId)
    {

        Proton.pn_message_set_id(_impl, convertToAtom(messageId));

    }

    @Override
    public int decode(byte[] data, int offset, int length)
    {
        return Proton.pn_message_decode(_impl, ByteBuffer.wrap(data,offset,length));
    }

    @Override
    public int encode(byte[] data, int offset, int length)
    {
        ByteBuffer buffer = ByteBuffer.wrap(data, offset, length);
        int status = Proton.pn_message_encode(_impl, buffer);
        return status == 0 ? buffer.position() : status;

    }

    @Override
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
    public void clear()
    {
        Proton.pn_message_clear(_impl);
    }

    @Override
    public MessageError getError()
    {
        return null;  //To change body of implemented methods use File | Settings | File Templates.
    }


    @Override
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

    }

    @Override
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

    public String toAMQPFormat(Object value)
    {
        //TODO
        throw new UnsupportedOperationException();
    }

    public Object parseAMQPFormat(String value)
    {
        //TODO
        throw new UnsupportedOperationException();
    }

    public void setFooter(Footer footer)
    {
        //TODO
        throw new UnsupportedOperationException();
    }


    public Header getHeader()
    {
        //TODO
        throw new UnsupportedOperationException();
    }

    public DeliveryAnnotations getDeliveryAnnotations()
    {
        //TODO
        throw new UnsupportedOperationException();
    }

    public MessageAnnotations getMessageAnnotations()
    {
        //TODO
        throw new UnsupportedOperationException();
    }

    public Properties getProperties()
    {
        //TODO
        throw new UnsupportedOperationException();
    }

    public ApplicationProperties getApplicationProperties()
    {
        //TODO
        throw new UnsupportedOperationException();
    }

    public Section getBody()
    {
        //TODO
        throw new UnsupportedOperationException();
    }

    public Footer getFooter()
    {
        //TODO
        throw new UnsupportedOperationException();
    }

    public void setHeader(Header header)
    {
        //TODO
        throw new UnsupportedOperationException();
    }

    public void setDeliveryAnnotations(DeliveryAnnotations deliveryAnnotations)
    {
        //TODO
        throw new UnsupportedOperationException();
    }

    public void setMessageAnnotations(MessageAnnotations messageAnnotations)
    {
        //TODO
        throw new UnsupportedOperationException();
    }

    public void setProperties(Properties properties)
    {
        //TODO
        throw new UnsupportedOperationException();
    }

    public void setApplicationProperties(ApplicationProperties applicationProperties)   
    {
        //TODO
        throw new UnsupportedOperationException();
    }

    public void setBody(Section body)
    {
        //TODO
        throw new UnsupportedOperationException();
    }


}
