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

import java.lang.reflect.Array;
import java.nio.ByteBuffer;
import java.util.*;

public class DecoderImpl implements ByteBufferDecoder
{

    private ByteBuffer _buffer;
    private PrimitiveTypeEncoding[] _constructors = new PrimitiveTypeEncoding[256];
    private Map<Object, DescribedTypeConstructor> _dynamicTypeConstructors =
            new HashMap<Object, DescribedTypeConstructor>();


    public DecoderImpl()
    {
    }


    DecoderImpl(final ByteBuffer buffer)
    {
        _buffer = buffer;
    }

    TypeConstructor readConstructor()
    {
        int code = ((int)readRawByte()) & 0xff;
        if(code == EncodingCodes.DESCRIBED_TYPE_INDICATOR)
        {
            final Object descriptor = readObject();
            TypeConstructor nestedEncoding = readConstructor();
            DescribedTypeConstructor dtc = _dynamicTypeConstructors.get(descriptor);
            if(dtc == null)
            {
                dtc = new DescribedTypeConstructor()
                {

                    public DescribedType newInstance(final Object described)
                    {
                        return new UnknownDescribedType(descriptor, described);
                    }

                    public Class getTypeClass()
                    {
                        return UnknownDescribedType.class;
                    }
                };
                register(descriptor, dtc);
            }
            return new DynamicTypeConstructor(dtc, nestedEncoding);
        }
        else
        {
            return _constructors[code];
        }
    }

    public void register(final Object descriptor, final DescribedTypeConstructor dtc)
    {
        _dynamicTypeConstructors.put(descriptor, dtc);
    }

    private ClassCastException unexpectedType(final Object val, Class clazz)
    {
        return new ClassCastException("Unexpected type "
                                      + val.getClass().getName()
                                      + ". Expected "
                                      + clazz.getName() +".");
    }


    public Boolean readBoolean()
    {
        return readBoolean(null);
    }

    public Boolean readBoolean(final Boolean defaultVal)
    {
        TypeConstructor constructor = readConstructor();
        Object val = constructor.readValue();
        if(val == null)
        {
            return defaultVal;
        }
        else if(val instanceof Boolean)
        {
            return (Boolean) val;
        }
        throw unexpectedType(val, Boolean.class);
    }

    public boolean readBoolean(final boolean defaultVal)
    {
        TypeConstructor constructor = readConstructor();
        if(constructor instanceof BooleanType.BooleanEncoding)
        {
            return ((BooleanType.BooleanEncoding)constructor).readPrimitiveValue();
        }
        else
        {
            Object val = constructor.readValue();
            if(val == null)
            {
                return defaultVal;
            }
            else
            {
                throw unexpectedType(val, Boolean.class);
            }
        }
    }

    public Byte readByte()
    {
        return readByte(null);
    }

    public Byte readByte(final Byte defaultVal)
    {
        TypeConstructor constructor = readConstructor();
        Object val = constructor.readValue();
        if(val == null)
        {
            return defaultVal;
        }
        else if(val instanceof Byte)
        {
            return (Byte) val;
        }
        throw unexpectedType(val, Byte.class);
    }

    public byte readByte(final byte defaultVal)
    {
        TypeConstructor constructor = readConstructor();
        if(constructor instanceof ByteType.ByteEncoding)
        {
            return ((ByteType.ByteEncoding)constructor).readPrimitiveValue();
        }
        else
        {
            Object val = constructor.readValue();
            if(val == null)
            {
                return defaultVal;
            }
            else
            {
                throw unexpectedType(val, Byte.class);
            }
        }
    }

    public Short readShort()
    {
        return readShort(null);
    }

    public Short readShort(final Short defaultVal)
    {
        TypeConstructor constructor = readConstructor();
        Object val = constructor.readValue();
        if(val == null)
        {
            return defaultVal;
        }
        else if(val instanceof Short)
        {
            return (Short) val;
        }
        throw unexpectedType(val, Short.class);

    }

    public short readShort(final short defaultVal)
    {

        TypeConstructor constructor = readConstructor();
        if(constructor instanceof ShortType.ShortEncoding)
        {
            return ((ShortType.ShortEncoding)constructor).readPrimitiveValue();
        }
        else
        {
            Object val = constructor.readValue();
            if(val == null)
            {
                return defaultVal;
            }
            else
            {
                throw unexpectedType(val, Short.class);
            }
        }
    }

    public Integer readInteger()
    {
        return readInteger(null);
    }

    public Integer readInteger(final Integer defaultVal)
    {
        TypeConstructor constructor = readConstructor();
        Object val = constructor.readValue();
        if(val == null)
        {
            return defaultVal;
        }
        else if(val instanceof Integer)
        {
            return (Integer) val;
        }
        throw unexpectedType(val, Integer.class);

    }

    public int readInteger(final int defaultVal)
    {

        TypeConstructor constructor = readConstructor();
        if(constructor instanceof IntegerType.IntegerEncoding)
        {
            return ((IntegerType.IntegerEncoding)constructor).readPrimitiveValue();
        }
        else
        {
            Object val = constructor.readValue();
            if(val == null)
            {
                return defaultVal;
            }
            else
            {
                throw unexpectedType(val, Integer.class);
            }
        }
    }

    public Long readLong()
    {
        return readLong(null);
    }

    public Long readLong(final Long defaultVal)
    {

        TypeConstructor constructor = readConstructor();
        Object val = constructor.readValue();
        if(val == null)
        {
            return defaultVal;
        }
        else if(val instanceof Long)
        {
            return (Long) val;
        }
        throw unexpectedType(val, Long.class);

    }

    public long readLong(final long defaultVal)
    {

        TypeConstructor constructor = readConstructor();
        if(constructor instanceof LongType.LongEncoding)
        {
            return ((LongType.LongEncoding)constructor).readPrimitiveValue();
        }
        else
        {
            Object val = constructor.readValue();
            if(val == null)
            {
                return defaultVal;
            }
            else
            {
                throw unexpectedType(val, Long.class);
            }
        }
    }

    public UnsignedByte readUnsignedByte()
    {
        return readUnsignedByte(null);
    }

    public UnsignedByte readUnsignedByte(final UnsignedByte defaultVal)
    {

        TypeConstructor constructor = readConstructor();
        Object val = constructor.readValue();
        if(val == null)
        {
            return defaultVal;
        }
        else if(val instanceof UnsignedByte)
        {
            return (UnsignedByte) val;
        }
        throw unexpectedType(val, UnsignedByte.class);

    }

    public UnsignedShort readUnsignedShort()
    {
        return readUnsignedShort(null);
    }

    public UnsignedShort readUnsignedShort(final UnsignedShort defaultVal)
    {

        TypeConstructor constructor = readConstructor();
        Object val = constructor.readValue();
        if(val == null)
        {
            return defaultVal;
        }
        else if(val instanceof UnsignedShort)
        {
            return (UnsignedShort) val;
        }
        throw unexpectedType(val, UnsignedShort.class);

    }

    public UnsignedInteger readUnsignedInteger()
    {
        return readUnsignedInteger(null);
    }

    public UnsignedInteger readUnsignedInteger(final UnsignedInteger defaultVal)
    {

        TypeConstructor constructor = readConstructor();
        Object val = constructor.readValue();
        if(val == null)
        {
            return defaultVal;
        }
        else if(val instanceof UnsignedInteger)
        {
            return (UnsignedInteger) val;
        }
        throw unexpectedType(val, UnsignedInteger.class);

    }

    public UnsignedLong readUnsignedLong()
    {
        return readUnsignedLong(null);
    }

    public UnsignedLong readUnsignedLong(final UnsignedLong defaultVal)
    {

        TypeConstructor constructor = readConstructor();
        Object val = constructor.readValue();
        if(val == null)
        {
            return defaultVal;
        }
        else if(val instanceof UnsignedLong)
        {
            return (UnsignedLong) val;
        }
        throw unexpectedType(val, UnsignedLong.class);

    }

    public Character readCharacter()
    {
        return readCharacter(null);
    }

    public Character readCharacter(final Character defaultVal)
    {

        TypeConstructor constructor = readConstructor();
        Object val = constructor.readValue();
        if(val == null)
        {
            return defaultVal;
        }
        else if(val instanceof Character)
        {
            return (Character) val;
        }
        throw unexpectedType(val, Character.class);

    }

    public char readCharacter(final char defaultVal)
    {

        TypeConstructor constructor = readConstructor();
        if(constructor instanceof CharacterType.CharacterEncoding)
        {
            return ((CharacterType.CharacterEncoding)constructor).readPrimitiveValue();
        }
        else
        {
            Object val = constructor.readValue();
            if(val == null)
            {
                return defaultVal;
            }
            else
            {
                throw unexpectedType(val, Character.class);
            }
        }
    }

    public Float readFloat()
    {
        return readFloat(null);
    }

    public Float readFloat(final Float defaultVal)
    {

        TypeConstructor constructor = readConstructor();
        Object val = constructor.readValue();
        if(val == null)
        {
            return defaultVal;
        }
        else if(val instanceof Float)
        {
            return (Float) val;
        }
        throw unexpectedType(val, Float.class);

    }

    public float readFloat(final float defaultVal)
    {

        TypeConstructor constructor = readConstructor();
        if(constructor instanceof FloatType.FloatEncoding)
        {
            return ((FloatType.FloatEncoding)constructor).readPrimitiveValue();
        }
        else
        {
            Object val = constructor.readValue();
            if(val == null)
            {
                return defaultVal;
            }
            else
            {
                throw unexpectedType(val, Float.class);
            }
        }
    }

    public Double readDouble()
    {
        return readDouble(null);
    }

    public Double readDouble(final Double defaultVal)
    {

        TypeConstructor constructor = readConstructor();
        Object val = constructor.readValue();
        if(val == null)
        {
            return defaultVal;
        }
        else if(val instanceof Double)
        {
            return (Double) val;
        }
        throw unexpectedType(val, Double.class);

    }

    public double readDouble(final double defaultVal)
    {

        TypeConstructor constructor = readConstructor();
        if(constructor instanceof DoubleType.DoubleEncoding)
        {
            return ((DoubleType.DoubleEncoding)constructor).readPrimitiveValue();
        }
        else
        {
            Object val = constructor.readValue();
            if(val == null)
            {
                return defaultVal;
            }
            else
            {
                throw unexpectedType(val, Double.class);
            }
        }
    }

    public UUID readUUID()
    {
        return readUUID(null);
    }

    public UUID readUUID(final UUID defaultVal)
    {

        TypeConstructor constructor = readConstructor();
        Object val = constructor.readValue();
        if(val == null)
        {
            return defaultVal;
        }
        else if(val instanceof UUID)
        {
            return (UUID) val;
        }
        throw unexpectedType(val, UUID.class);

    }

    public Decimal32 readDecimal32()
    {
        return readDecimal32(null);
    }

    public Decimal32 readDecimal32(final Decimal32 defaultValue)
    {

        TypeConstructor constructor = readConstructor();
        Object val = constructor.readValue();
        if(val == null)
        {
            return defaultValue;
        }
        else if(val instanceof Decimal32)
        {
            return (Decimal32) val;
        }
        throw unexpectedType(val, Decimal32.class);

    }

    public Decimal64 readDecimal64()
    {
        return readDecimal64(null);
    }

    public Decimal64 readDecimal64(final Decimal64 defaultValue)
    {

        TypeConstructor constructor = readConstructor();
        Object val = constructor.readValue();
        if(val == null)
        {
            return defaultValue;
        }
        else if(val instanceof Decimal64)
        {
            return (Decimal64) val;
        }
        throw unexpectedType(val, Decimal64.class);
    }

    public Decimal128 readDecimal128()
    {
        return readDecimal128(null);
    }

    public Decimal128 readDecimal128(final Decimal128 defaultValue)
    {

        TypeConstructor constructor = readConstructor();
        Object val = constructor.readValue();
        if(val == null)
        {
            return defaultValue;
        }
        else if(val instanceof Decimal128)
        {
            return (Decimal128) val;
        }
        throw unexpectedType(val, Decimal128.class);
    }

    public Date readTimestamp()
    {
        return readTimestamp(null);
    }

    public Date readTimestamp(final Date defaultValue)
    {

        TypeConstructor constructor = readConstructor();
        Object val = constructor.readValue();
        if(val == null)
        {
            return defaultValue;
        }
        else if(val instanceof Date)
        {
            return (Date) val;
        }
        throw unexpectedType(val, Date.class);
    }

    public Binary readBinary()
    {
        return readBinary(null);
    }

    public Binary readBinary(final Binary defaultValue)
    {

        TypeConstructor constructor = readConstructor();
        Object val = constructor.readValue();
        if(val == null)
        {
            return defaultValue;
        }
        else if(val instanceof Binary)
        {
            return (Binary) val;
        }
        throw unexpectedType(val, Binary.class);
    }

    public Symbol readSymbol()
    {
        return readSymbol(null);
    }

    public Symbol readSymbol(final Symbol defaultValue)
    {

        TypeConstructor constructor = readConstructor();
        Object val = constructor.readValue();
        if(val == null)
        {
            return defaultValue;
        }
        else if(val instanceof Symbol)
        {
            return (Symbol) val;
        }
        throw unexpectedType(val, Symbol.class);
    }

    public String readString()
    {
        return readString(null);
    }

    public String readString(final String defaultValue)
    {

        TypeConstructor constructor = readConstructor();
        Object val = constructor.readValue();
        if(val == null)
        {
            return defaultValue;
        }
        else if(val instanceof String)
        {
            return (String) val;
        }
        throw unexpectedType(val, String.class);
    }

    public List readList()
    {

        TypeConstructor constructor = readConstructor();
        Object val = constructor.readValue();
        if(val == null)
        {
            return null;
        }
        else if(val instanceof List)
        {
            return (List) val;
        }
        throw unexpectedType(val, List.class);
    }

    public <T> void readList(final ListProcessor<T> processor)
    {
        //TODO.
    }

    public Map readMap()
    {

        TypeConstructor constructor = readConstructor();
        Object val = constructor.readValue();
        if(val == null)
        {
            return null;
        }
        else if(val instanceof Map)
        {
            return (Map) val;
        }
        throw unexpectedType(val, Map.class);
    }

    public <T> T[] readArray(final Class<T> clazz)
    {
        return null;  //TODO.
    }

    public Object[] readArray()
    {
        return (Object[]) readConstructor().readValue();

    }

    public boolean[] readBooleanArray()
    {
        return (boolean[]) ((ArrayType.ArrayEncoding)readConstructor()).readValueArray();
    }

    public byte[] readByteArray()
    {
        return (byte[]) ((ArrayType.ArrayEncoding)readConstructor()).readValueArray();
    }

    public short[] readShortArray()
    {
        return (short[]) ((ArrayType.ArrayEncoding)readConstructor()).readValueArray();
    }

    public int[] readIntegerArray()
    {
        return (int[]) ((ArrayType.ArrayEncoding)readConstructor()).readValueArray();
    }

    public long[] readLongArray()
    {
        return (long[]) ((ArrayType.ArrayEncoding)readConstructor()).readValueArray();
    }

    public float[] readFloatArray()
    {
        return (float[]) ((ArrayType.ArrayEncoding)readConstructor()).readValueArray();
    }

    public double[] readDoubleArray()
    {
        return (double[]) ((ArrayType.ArrayEncoding)readConstructor()).readValueArray();
    }

    public char[] readCharacterArray()
    {
        return (char[]) ((ArrayType.ArrayEncoding)readConstructor()).readValueArray();
    }

    public <T> T[] readMultiple(final Class<T> clazz)
    {
        Object val = readObject();
        if(val == null)
        {
            return null;
        }
        else if(val.getClass().isArray())
        {
            if(clazz.isAssignableFrom(val.getClass().getComponentType()))
            {
                return (T[]) val;
            }
            else
            {
                throw unexpectedType(val, Array.newInstance(clazz, 0).getClass());
            }
        }
        else if(clazz.isAssignableFrom(val.getClass()))
        {
            T[] array = (T[]) Array.newInstance(clazz, 1);
            array[0] = (T) val;
            return array;
        }
        else
        {
            throw unexpectedType(val, Array.newInstance(clazz, 0).getClass());
        }
    }

    public Object[] readMultiple()
    {
        Object val = readObject();
        if(val == null)
        {
            return null;
        }
        else if(val.getClass().isArray())
        {
            return (Object[]) val;
        }
        else
        {
            Object[] array = (Object[]) Array.newInstance(val.getClass(), 1);
            array[0] = val;
            return array;
        }
    }

    public byte[] readByteMultiple()
    {
        return new byte[0];  //TODO.
    }

    public short[] readShortMultiple()
    {
        return new short[0];  //TODO.
    }

    public int[] readIntegerMultiple()
    {
        return new int[0];  //TODO.
    }

    public long[] readLongMultiple()
    {
        return new long[0];  //TODO.
    }

    public float[] readFloatMultiple()
    {
        return new float[0];  //TODO.
    }

    public double[] readDoubleMultiple()
    {
        return new double[0];  //TODO.
    }

    public char[] readCharacterMultiple()
    {
        return new char[0];  //TODO.
    }

    public Object readObject()
    {
        TypeConstructor constructor = readConstructor();
        if(constructor== null)
        {
            throw new DecodeException("Unknown constructor");
        }
        return constructor instanceof ArrayType.ArrayEncoding
               ? ((ArrayType.ArrayEncoding)constructor).readValueArray()
               : constructor.readValue();
    }

    public Object readObject(final Object defaultValue)
    {
        Object val = readObject();
        return val == null ? defaultValue : val;
    }

    <V> void register(PrimitiveType<V> type)
    {
        Collection<? extends PrimitiveTypeEncoding<V>> encodings = type.getAllEncodings();

        for(PrimitiveTypeEncoding<V> encoding : encodings)
        {
            _constructors[((int) encoding.getEncodingCode()) & 0xFF ] = encoding;
        }

    }

    byte readRawByte()
    {
        return _buffer.get();
    }

    int readRawInt()
    {
        return _buffer.getInt();
    }

    long readRawLong()
    {
        return _buffer.getLong();
    }

    short readRawShort()
    {
        return _buffer.getShort();
    }

    float readRawFloat()
    {
        return _buffer.getFloat();
    }

    double readRawDouble()
    {
        return _buffer.getDouble();
    }

    void readRaw(final byte[] data, final int offset, final int length)
    {
        _buffer.get(data, offset, length);
    }


    <V> V readRaw(TypeDecoder<V> decoder, int size)
    {
        V decode = decoder.decode((ByteBuffer) _buffer.slice().limit(size));
        _buffer.position(_buffer.position()+size);
        return decode;
    }

    public void setByteBuffer(final ByteBuffer buffer)
    {
        _buffer = buffer;
    }

    interface TypeDecoder<V>
    {
        V decode(ByteBuffer buf);
    }

    private static class UnknownDescribedType implements DescribedType
    {
        private final Object _descriptor;
        private final Object _described;

        public UnknownDescribedType(final Object descriptor, final Object described)
        {
            _descriptor = descriptor;
            _described = described;
        }

        public Object getDescriptor()
        {
            return _descriptor;
        }

        public Object getDescribed()
        {
            return _described;
        }


        @Override
        public boolean equals(Object obj)
        {

            return obj instanceof DescribedType
                   && _descriptor == null ? ((DescribedType) obj).getDescriptor() == null
                                         : _descriptor.equals(((DescribedType) obj).getDescriptor())
                   && _described == null ?  ((DescribedType) obj).getDescribed() == null
                                         : _described.equals(((DescribedType) obj).getDescribed());

        }

    }
}
