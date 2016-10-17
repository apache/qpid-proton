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
package org.apache.qpid.proton.codec;

import java.util.Date;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;

import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.Decimal128;
import org.apache.qpid.proton.amqp.Decimal32;
import org.apache.qpid.proton.amqp.Decimal64;
import org.apache.qpid.proton.amqp.DescribedType;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.UnsignedByte;
import org.apache.qpid.proton.amqp.UnsignedInteger;
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.UnsignedShort;

public final class EncoderImpl implements ByteBufferEncoder
{
    private static final byte DESCRIBED_TYPE_OP = (byte)0;


    private final Map<Class, AMQPType> _typeRegistry = new ConcurrentHashMap<>();
    private Map<Object, AMQPType> _describedDescriptorRegistry = new ConcurrentHashMap<>();
    private Map<Class, AMQPType>  _describedTypesClassRegistry = new ConcurrentHashMap<>();

    private final NullType              _nullType;
    private final BooleanType           _booleanType;
    private final ByteType              _byteType;
    private final UnsignedByteType      _unsignedByteType;
    private final ShortType             _shortType;
    private final UnsignedShortType     _unsignedShortType;
    private final IntegerType           _integerType;
    private final UnsignedIntegerType   _unsignedIntegerType;
    private final LongType              _longType;
    private final UnsignedLongType      _unsignedLongType;
    private final BigIntegerType        _bigIntegerType;

    private final CharacterType         _characterType;
    private final FloatType             _floatType;
    private final DoubleType            _doubleType;
    private final TimestampType         _timestampType;
    private final UUIDType              _uuidType;

    private final Decimal32Type         _decimal32Type;
    private final Decimal64Type         _decimal64Type;
    private final Decimal128Type        _decimal128Type;

    private final BinaryType            _binaryType;
    private final SymbolType            _symbolType;
    private final StringType            _stringType;

    private final ListType              _listType;
    private final MapType               _mapType;

    private final ArrayType             _arrayType;

    public EncoderImpl(DecoderImpl decoder)
    {

        _nullType               = new NullType(this, decoder);
        _booleanType            = new BooleanType(this, decoder);
        _byteType               = new ByteType(this, decoder);
        _unsignedByteType       = new UnsignedByteType(this, decoder);
        _shortType              = new ShortType(this, decoder);
        _unsignedShortType      = new UnsignedShortType(this, decoder);
        _integerType            = new IntegerType(this, decoder);
        _unsignedIntegerType    = new UnsignedIntegerType(this, decoder);
        _longType               = new LongType(this, decoder);
        _unsignedLongType       = new UnsignedLongType(this, decoder);
        _bigIntegerType         = new BigIntegerType(this, decoder);

        _characterType          = new CharacterType(this, decoder);
        _floatType              = new FloatType(this, decoder);
        _doubleType             = new DoubleType(this, decoder);
        _timestampType          = new TimestampType(this, decoder);
        _uuidType               = new UUIDType(this, decoder);

        _decimal32Type          = new Decimal32Type(this, decoder);
        _decimal64Type          = new Decimal64Type(this, decoder);
        _decimal128Type         = new Decimal128Type(this, decoder);


        _binaryType             = new BinaryType(this, decoder);
        _symbolType             = new SymbolType(this, decoder);
        _stringType             = new StringType(this, decoder);

        _listType               = new ListType(this, decoder);
        _mapType                = new MapType(this, decoder);

        _arrayType              = new ArrayType(this,
                                                decoder,
                                                _booleanType,
                                                _byteType,
                                                _shortType,
                                                _integerType,
                                                _longType,
                                                _floatType,
                                                _doubleType,
                                                _characterType);


    }

    @Override
    public AMQPType getType(final Object element)
    {
        if(element instanceof DescribedType)
        {
            AMQPType amqpType;

            Object descriptor = ((DescribedType)element).getDescriptor();
            amqpType = _describedDescriptorRegistry.get(descriptor);
            if(amqpType == null)
            {
                amqpType = new DynamicDescribedType(this, descriptor);
                _describedDescriptorRegistry.put(descriptor, amqpType);
            }
            return amqpType;

        }
        else
        {
            return getTypeFromClass(element == null ? Void.class : element.getClass());
        }
    }

    public AMQPType getTypeFromClass(final Class clazz)
    {
        AMQPType amqpType = _typeRegistry.get(clazz);
        if(amqpType == null)
        {

            if(clazz.isArray())
            {
                amqpType = _arrayType;
            }
            else
            {
                if(List.class.isAssignableFrom(clazz))
                {
                    amqpType = _listType;
                }
                else if(Map.class.isAssignableFrom(clazz))
                {
                    amqpType = _mapType;
                }
                else if(DescribedType.class.isAssignableFrom(clazz))
                {
                    amqpType = _describedTypesClassRegistry.get(clazz);
                }
            }
            _typeRegistry.put(clazz, amqpType);
        }
        return amqpType;
    }

    @Override
    public <V> void register(AMQPType<V> type)
    {
        register(type.getTypeClass(), type);
    }

    <T> void register(Class<T> clazz, AMQPType<T> type)
    {
        _typeRegistry.put(clazz, type);
    }

    public void registerDescribedType(Class clazz, Object descriptor)
    {
        AMQPType type = _describedDescriptorRegistry.get(descriptor);
        if(type == null)
        {
            type = new DynamicDescribedType(this, descriptor);
            _describedDescriptorRegistry.put(descriptor, type);
        }
        _describedTypesClassRegistry.put(clazz, type);
    }

    public void writeNull(WritableBuffer buffer)
    {
        _nullType.write(buffer);
    }

    public void writeBoolean(WritableBuffer buffer, final boolean bool)
    {
        _booleanType.writeValue(buffer, bool);
    }

    public void writeBoolean(WritableBuffer buffer, final Boolean bool)
    {
        if(bool == null)
        {
            writeNull(buffer);
        }
        else
        {
            _booleanType.write(buffer, bool);
        }
    }

    public void writeUnsignedByte(WritableBuffer buffer, final UnsignedByte ubyte)
    {
        if(ubyte == null)
        {
            writeNull(buffer);
        }
        else
        {
            _unsignedByteType.write(buffer, ubyte);
        }
    }

    public void writeUnsignedShort(WritableBuffer buffer, final UnsignedShort ushort)
    {
        if(ushort == null)
        {
            writeNull(buffer);
        }
        else
        {
            _unsignedShortType.write(buffer, ushort);
        }
    }

    public void writeUnsignedInteger(WritableBuffer buffer, final UnsignedInteger uint)
    {
        if(uint == null)
        {
            writeNull(buffer);
        }
        else
        {
            _unsignedIntegerType.write(buffer, uint);
        }
    }

    public void writeUnsignedLong(WritableBuffer buffer, final UnsignedLong ulong)
    {
        if(ulong == null)
        {
            writeNull(buffer);
        }
        else
        {
            _unsignedLongType.write(buffer, ulong);
        }
    }

    public void writeByte(WritableBuffer buffer, final byte b)
    {
        _byteType.write(buffer, b);
    }

    public void writeByte(WritableBuffer buffer, final Byte b)
    {
        if(b == null)
        {
            writeNull(buffer);
        }
        else
        {
            writeByte(buffer, b.byteValue());
        }
    }

    public void writeShort(WritableBuffer buffer, final short s)
    {
        _shortType.write(buffer, s);
    }

    public void writeShort(WritableBuffer buffer, final Short s)
    {
        if(s == null)
        {
            writeNull(buffer);
        }
        else
        {
            writeShort(buffer, s.shortValue());
        }
    }

    public void writeInteger(WritableBuffer buffer, final int i)
    {
        _integerType.write(buffer, i);
    }

    public void writeInteger(WritableBuffer buffer, final Integer i)
    {
        if(i == null)
        {
            writeNull(buffer);
        }
        else
        {
            writeInteger(buffer, i.intValue());
        }
    }

    public void writeLong(WritableBuffer buffer, final long l)
    {
        _longType.write(buffer, l);
    }

    public void writeLong(WritableBuffer buffer, final Long l)
    {

        if(l == null)
        {
            writeNull(buffer);
        }
        else
        {
            writeLong(buffer, l.longValue());
        }
    }

    public void writeFloat(WritableBuffer buffer, final float f)
    {
        _floatType.write(buffer, f);
    }

    public void writeFloat(WritableBuffer buffer, final Float f)
    {
        if(f == null)
        {
            writeNull(buffer);
        }
        else
        {
            writeFloat(buffer, f.floatValue());
        }
    }

    public void writeDouble(WritableBuffer buffer, final double d)
    {
        _doubleType.write(buffer, d);
    }

    public void writeDouble(WritableBuffer buffer, final Double d)
    {
        if(d == null)
        {
            writeNull(buffer);
        }
        else
        {
            writeDouble(buffer, d.doubleValue());
        }
    }

    public void writeDecimal32(WritableBuffer buffer, final Decimal32 d)
    {
        if(d == null)
        {
            writeNull(buffer);
        }
        else
        {
            _decimal32Type.write(buffer, d);
        }
    }

    public void writeDecimal64(WritableBuffer buffer, final Decimal64 d)
    {
        if(d == null)
        {
            writeNull(buffer);
        }
        else
        {
            _decimal64Type.write(buffer, d);
        }
    }

    public void writeDecimal128(WritableBuffer buffer, final Decimal128 d)
    {
        if(d == null)
        {
            writeNull(buffer);
        }
        else
        {
            _decimal128Type.write(buffer, d);
        }
    }

    public void writeCharacter(WritableBuffer buffer, final char c)
    {
        // TODO - java character may be half of a pair, should probably throw exception then
        _characterType.write(buffer, c);
    }

    public void writeCharacter(WritableBuffer buffer, final Character c)
    {
        if(c == null)
        {
            writeNull(buffer);
        }
        else
        {
            writeCharacter(buffer, c.charValue());
        }
    }

    public void writeTimestamp(WritableBuffer buffer, final long d)
    {
        _timestampType.write(buffer, d);
    }

    public void writeTimestamp(WritableBuffer buffer, final Date d)
    {
        if(d == null)
        {
            writeNull(buffer);
        }
        else
        {
            writeTimestamp(buffer, d.getTime());
        }
    }

    public void writeUUID(WritableBuffer buffer, final UUID uuid)
    {
        if(uuid == null)
        {
            writeNull(buffer);
        }
        else
        {
            _uuidType.write(buffer, uuid);
        }

    }

    public void writeBinary(WritableBuffer buffer, final Binary b)
    {
        if(b == null)
        {
            writeNull(buffer);
        }
        else
        {
            _binaryType.write(buffer, b);
        }
    }

    public void writeString(WritableBuffer buffer, final String s)
    {
        if(s == null)
        {
            writeNull(buffer);
        }
        else
        {
            _stringType.write(buffer, s);
        }
    }

    public void writeSymbol(WritableBuffer buffer, final Symbol s)
    {
        if(s == null)
        {
            writeNull(buffer);
        }
        else
        {
            _symbolType.write(buffer, s);
        }

    }

    public void writeList(WritableBuffer buffer, final List l)
    {
        if(l == null)
        {
            writeNull(buffer);
        }
        else
        {
            _listType.write(buffer, l);
        }
    }

    public void writeMap(WritableBuffer buffer, final Map m)
    {

        if(m == null)
        {
            writeNull(buffer);
        }
        else
        {
            _mapType.write(buffer, m);
        }
    }

    public void writeDescribedType(WritableBuffer buffer, final DescribedType d)
    {
        if(d == null)
        {
            writeNull(buffer);
        }
        else
        {
            buffer.put(DESCRIBED_TYPE_OP);
            writeObject(buffer, d.getDescriptor());
            writeObject(buffer, d.getDescribed());
        }
    }

    public void writeArray(WritableBuffer buffer, final boolean[] a)
    {
        if(a == null)
        {
            writeNull(buffer);
        }
        else
        {
            _arrayType.write(buffer, a);
        }
    }

    public void writeArray(WritableBuffer buffer, final byte[] a)
    {
        if(a == null)
        {
            writeNull(buffer);
        }
        else
        {
            _arrayType.write(buffer, a);
        }
    }

    public void writeArray(WritableBuffer buffer, final short[] a)
    {
        if(a == null)
        {
            writeNull(buffer);
        }
        else
        {
            _arrayType.write(buffer, a);
        }
    }

    public void writeArray(WritableBuffer buffer, final int[] a)
    {
        if(a == null)
        {
            writeNull(buffer);
        }
        else
        {
            _arrayType.write(buffer, a);
        }
    }

    public void writeArray(WritableBuffer buffer, final long[] a)
    {
        if(a == null)
        {
            writeNull(buffer);
        }
        else
        {
            _arrayType.write(buffer, a);
        }
    }

    public void writeArray(WritableBuffer buffer, final float[] a)
    {
        if(a == null)
        {
            writeNull(buffer);
        }
        else
        {
            _arrayType.write(buffer, a);
        }
    }

    public void writeArray(WritableBuffer buffer, final double[] a)
    {
        if(a == null)
        {
            writeNull(buffer);
        }
        else
        {
            _arrayType.write(buffer, a);
        }
    }

    public void writeArray(WritableBuffer buffer, final char[] a)
    {
        if(a == null)
        {
            writeNull(buffer);
        }
        else
        {
            _arrayType.write(buffer, a);
        }
    }

    public void writeArray(WritableBuffer buffer, final Object[] a)
    {
        if(a == null)
        {
            writeNull(buffer);
        }
        else
        {
            _arrayType.write(buffer, a);
        }
    }

    public void writeObject(WritableBuffer buffer, final Object o)
    {
        AMQPType type = _typeRegistry.get(o == null ? Void.class : o.getClass());

        if(type == null)
        {
            if(o.getClass().isArray())
            {
                Class<?> componentType = o.getClass().getComponentType();
                if(componentType.isPrimitive())
                {
                    if(componentType == Boolean.TYPE)
                    {
                        writeArray(buffer, (boolean[])o);
                    }
                    else if(componentType == Byte.TYPE)
                    {
                        writeArray(buffer, (byte[])o);
                    }
                    else if(componentType == Short.TYPE)
                    {
                        writeArray(buffer, (short[])o);
                    }
                    else if(componentType == Integer.TYPE)
                    {
                        writeArray(buffer, (int[])o);
                    }
                    else if(componentType == Long.TYPE)
                    {
                        writeArray(buffer, (long[])o);
                    }
                    else if(componentType == Float.TYPE)
                    {
                        writeArray(buffer, (float[])o);
                    }
                    else if(componentType == Double.TYPE)
                    {
                        writeArray(buffer, (double[])o);
                    }
                    else if(componentType == Character.TYPE)
                    {
                        writeArray(buffer, (char[])o);
                    }
                    else
                    {
                        throw new IllegalArgumentException("Cannot write arrays of type " + componentType.getName());
                    }
                }
                else
                {
                    writeArray(buffer, (Object[]) o);
                }
            }
            else if(o instanceof List)
            {
                writeList(buffer, (List)o);
            }
            else if(o instanceof Map)
            {
                writeMap(buffer, (Map)o);
            }
            else if(o instanceof DescribedType)
            {
                writeDescribedType(buffer, (DescribedType)o);
            }
            else
            {
                throw new IllegalArgumentException("Do not know how to write Objects of class " + o.getClass()
                                                                                                       .getName());

            }
        }
        else
        {
            type.write(buffer, o);
        }
    }

    public void writeRaw(WritableBuffer buffer, final byte b)
    {
        buffer.put(b);
    }

    void writeRaw(WritableBuffer buffer, final short s)
    {
        buffer.putShort(s);
    }

    void writeRaw(WritableBuffer buffer, final int i)
    {
        buffer.putInt(i);
    }

    void writeRaw(WritableBuffer buffer, final long l)
    {
        buffer.putLong(l);
    }

    void writeRaw(WritableBuffer buffer, final float f)
    {
        buffer.putFloat(f);
    }

    void writeRaw(WritableBuffer buffer, final double d)
    {
        buffer.putDouble(d);
    }

    void writeRaw(WritableBuffer buffer, byte[] src, int offset, int length)
    {
        buffer.put(src, offset, length);
    }

    void writeRaw(WritableBuffer _buffer, String string)
    {
        final int length = string.length();
        int c;

        for (int i = 0; i < length; i++)
        {
            c = string.charAt(i);
            if ((c & 0xFF80) == 0)          /* U+0000..U+007F */
            {
                _buffer.put((byte) c);
            }
            else if ((c & 0xF800) == 0)     /* U+0080..U+07FF */
            {
                _buffer.put((byte)(0xC0 | ((c >> 6) & 0x1F)));
                _buffer.put((byte)(0x80 | (c & 0x3F)));
            }
            else if ((c & 0xD800) != 0xD800 || (c > 0xDBFF))     /* U+0800..U+FFFF - excluding surrogate pairs */
            {
                _buffer.put((byte)(0xE0 | ((c >> 12) & 0x0F)));
                _buffer.put((byte)(0x80 | ((c >> 6) & 0x3F)));
                _buffer.put((byte)(0x80 | (c & 0x3F)));
            }
            else
            {
                int low;

                if((++i == length) || ((low = string.charAt(i)) & 0xDC00) != 0xDC00)
                {
                    throw new IllegalArgumentException("String contains invalid Unicode code points");
                }

                c = 0x010000 + ((c & 0x03FF) << 10) + (low & 0x03FF);

                _buffer.put((byte)(0xF0 | ((c >> 18) & 0x07)));
                _buffer.put((byte)(0x80 | ((c >> 12) & 0x3F)));
                _buffer.put((byte)(0x80 | ((c >> 6) & 0x3F)));
                _buffer.put((byte)(0x80 | (c & 0x3F)));
            }
        }
    }
}
