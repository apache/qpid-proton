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
import java.util.*;

public class DecoderImpl implements ByteBufferDecoder
{

    private PrimitiveTypeEncoding[] _constructors = new PrimitiveTypeEncoding[256];
    private Map<Object, DescribedTypeConstructor> _dynamicTypeConstructors =
            new HashMap<Object, DescribedTypeConstructor>();
    private DescribedTypeConstructor cacheDTC[] = new DescribedTypeConstructor[150];

    public DecoderImpl()
    {
    }

    public TypeConstructor readConstructor(ReadableBuffer buffer)
    {
        final int code = ((int)readRawByte(buffer)) & 0xff;
        if(code == EncodingCodes.DESCRIBED_TYPE_INDICATOR)
        {
            final Object descriptor = readObject(buffer);
            DescribedTypeConstructor dtc = null;

            if (descriptor instanceof UnsignedLong)
            {
                long value = ((UnsignedLong)descriptor).intValue();

                if (value < cacheDTC.length)
                {
                    dtc = cacheDTC[(int)value];
                }
            }

            if (dtc == null)
            {
                dtc = _dynamicTypeConstructors.get(descriptor);
            }

            TypeConstructor nestedEncoding = readConstructor(buffer);

            if(dtc == null)
            {
                dtc = new DescribedTypeConstructor()
                {

                    public DescribedType newInstance(ReadableBuffer buffer, final TypeConstructor constructor)
                    {
                        return new UnknownDescribedType(descriptor, constructor.readValue(buffer));
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

        // Caching most entities with an array. this is faster than hashmap
        if (descriptor instanceof  UnsignedLong)
        {
            long value = ((UnsignedLong) descriptor).longValue();
            if (value < cacheDTC.length)
            {
                cacheDTC[(int)value] = dtc;
            }
        }
    }

    private ClassCastException unexpectedType(final Object val, Class clazz)
    {
        return new ClassCastException("Unexpected type "
                                      + val.getClass().getName()
                                      + ". Expected "
                                      + clazz.getName() +".");
    }


    public Boolean readBoolean(final ReadableBuffer buffer)
    {
        return readBoolean(buffer, null);
    }

    public Boolean readBoolean(final ReadableBuffer buffer, final Boolean defaultVal)
    {
        TypeConstructor constructor = readConstructor(buffer);
        Object val = constructor.readValue(buffer);
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

    public boolean readBoolean(ReadableBuffer buffer, final boolean defaultVal)
    {
        TypeConstructor constructor = readConstructor(buffer);
        if(constructor instanceof BooleanType.BooleanEncoding)
        {
            return ((BooleanType.BooleanEncoding)constructor).readPrimitiveValue(buffer);
        }
        else
        {
            Object val = constructor.readValue(buffer);
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

    public Byte readByte(ReadableBuffer buffer)
    {
        return readByte(buffer, null);
    }

    public Byte readByte(ReadableBuffer buffer, final Byte defaultVal)
    {
        TypeConstructor constructor = readConstructor(buffer);
        Object val = constructor.readValue(buffer);
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

    public byte readByte(ReadableBuffer buffer, final byte defaultVal)
    {
        TypeConstructor constructor = readConstructor(buffer);
        if(constructor instanceof ByteType.ByteEncoding)
        {
            return ((ByteType.ByteEncoding)constructor).readPrimitiveValue(buffer);
        }
        else
        {
            Object val = constructor.readValue(buffer);
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

    public Short readShort(ReadableBuffer buffer)
    {
        return readShort(buffer, null);
    }

    public Short readShort(ReadableBuffer buffer, final Short defaultVal)
    {
        TypeConstructor constructor = readConstructor(buffer);
        Object val = constructor.readValue(buffer);
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

    public short readShort(ReadableBuffer buffer, final short defaultVal)
    {

        TypeConstructor constructor = readConstructor(buffer);
        if(constructor instanceof ShortType.ShortEncoding)
        {
            return ((ShortType.ShortEncoding)constructor).readPrimitiveValue(buffer);
        }
        else
        {
            Object val = constructor.readValue(buffer);
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

    public Integer readInteger(ReadableBuffer buffer)
    {
        return readInteger(buffer, null);
    }

    public Integer readInteger(ReadableBuffer buffer, final Integer defaultVal)
    {
        TypeConstructor constructor = readConstructor(buffer);
        Object val = constructor.readValue(buffer);
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

    public int readInteger(ReadableBuffer buffer, final int defaultVal)
    {

        TypeConstructor constructor = readConstructor(buffer);
        if(constructor instanceof IntegerType.IntegerEncoding)
        {
            return ((IntegerType.IntegerEncoding)constructor).readPrimitiveValue(buffer);
        }
        else
        {
            Object val = constructor.readValue(buffer);
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

    public Long readLong(ReadableBuffer buffer)
    {
        return readLong(buffer, null);
    }

    public Long readLong(ReadableBuffer buffer, final Long defaultVal)
    {

        TypeConstructor constructor = readConstructor(buffer);
        Object val = constructor.readValue(buffer);
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

    public long readLong(ReadableBuffer buffer, final long defaultVal)
    {

        TypeConstructor constructor = readConstructor(buffer);
        if(constructor instanceof LongType.LongEncoding)
        {
            return ((LongType.LongEncoding)constructor).readPrimitiveValue(buffer);
        }
        else
        {
            Object val = constructor.readValue(buffer);
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

    public UnsignedByte readUnsignedByte(ReadableBuffer buffer)
    {
        return readUnsignedByte(buffer, null);
    }

    public UnsignedByte readUnsignedByte(ReadableBuffer buffer, final UnsignedByte defaultVal)
    {

        TypeConstructor constructor = readConstructor(buffer);
        Object val = constructor.readValue(buffer);
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

    public UnsignedShort readUnsignedShort(ReadableBuffer buffer)
    {
        return readUnsignedShort(buffer, null);
    }

    public UnsignedShort readUnsignedShort(ReadableBuffer buffer, final UnsignedShort defaultVal)
    {

        TypeConstructor constructor = readConstructor(buffer);
        Object val = constructor.readValue(buffer);
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

    public UnsignedInteger readUnsignedInteger(ReadableBuffer buffer)
    {
        return readUnsignedInteger(buffer, null);
    }

    public UnsignedInteger readUnsignedInteger(ReadableBuffer buffer, final UnsignedInteger defaultVal)
    {

        TypeConstructor constructor = readConstructor(buffer);
        Object val = constructor.readValue(buffer);
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

    public UnsignedLong readUnsignedLong(ReadableBuffer buffer)
    {
        return readUnsignedLong(buffer, null);
    }

    public UnsignedLong readUnsignedLong(ReadableBuffer buffer, final UnsignedLong defaultVal)
    {

        TypeConstructor constructor = readConstructor(buffer);
        Object val = constructor.readValue(buffer);
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

    public Character readCharacter(ReadableBuffer buffer)
    {
        return readCharacter(buffer, null);
    }

    public Character readCharacter(ReadableBuffer buffer, final Character defaultVal)
    {

        TypeConstructor constructor = readConstructor(buffer);
        Object val = constructor.readValue(buffer);
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

    public char readCharacter(ReadableBuffer buffer, final char defaultVal)
    {

        TypeConstructor constructor = readConstructor(buffer);
        if(constructor instanceof CharacterType.CharacterEncoding)
        {
            return ((CharacterType.CharacterEncoding)constructor).readPrimitiveValue(buffer);
        }
        else
        {
            Object val = constructor.readValue(buffer);
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

    public Float readFloat(ReadableBuffer buffer)
    {
        return readFloat(buffer, null);
    }

    public Float readFloat(ReadableBuffer buffer, final Float defaultVal)
    {

        TypeConstructor constructor = readConstructor(buffer);
        Object val = constructor.readValue(buffer);
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

    public float readFloat(ReadableBuffer buffer, final float defaultVal)
    {

        TypeConstructor constructor = readConstructor(buffer);
        if(constructor instanceof FloatType.FloatEncoding)
        {
            return ((FloatType.FloatEncoding)constructor).readPrimitiveValue(buffer);
        }
        else
        {
            Object val = constructor.readValue(buffer);
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

    public Double readDouble(ReadableBuffer buffer)
    {
        return readDouble(buffer, null);
    }

    public Double readDouble(ReadableBuffer buffer, final Double defaultVal)
    {

        TypeConstructor constructor = readConstructor(buffer);
        Object val = constructor.readValue(buffer);
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

    public double readDouble(ReadableBuffer buffer, final double defaultVal)
    {

        TypeConstructor constructor = readConstructor(buffer);
        if(constructor instanceof DoubleType.DoubleEncoding)
        {
            return ((DoubleType.DoubleEncoding)constructor).readPrimitiveValue(buffer);
        }
        else
        {
            Object val = constructor.readValue(buffer);
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

    public UUID readUUID(ReadableBuffer buffer)
    {
        return readUUID(buffer, null);
    }

    public UUID readUUID(ReadableBuffer buffer, final UUID defaultVal)
    {

        TypeConstructor constructor = readConstructor(buffer);
        Object val = constructor.readValue(buffer);
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

    public Decimal32 readDecimal32(ReadableBuffer buffer)
    {
        return readDecimal32(buffer, null);
    }

    public Decimal32 readDecimal32(ReadableBuffer buffer, final Decimal32 defaultValue)
    {

        TypeConstructor constructor = readConstructor(buffer);
        Object val = constructor.readValue(buffer);
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

    public Decimal64 readDecimal64(ReadableBuffer buffer)
    {
        return readDecimal64(buffer, null);
    }

    public Decimal64 readDecimal64(ReadableBuffer buffer, final Decimal64 defaultValue)
    {

        TypeConstructor constructor = readConstructor(buffer);
        Object val = constructor.readValue(buffer);
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

    public Decimal128 readDecimal128(ReadableBuffer buffer)
    {
        return readDecimal128(buffer, null);
    }

    public Decimal128 readDecimal128(ReadableBuffer buffer, final Decimal128 defaultValue)
    {

        TypeConstructor constructor = readConstructor(buffer);
        Object val = constructor.readValue(buffer);
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

    public Date readTimestamp(ReadableBuffer buffer)
    {
        return readTimestamp(buffer, null);
    }

    public Date readTimestamp(ReadableBuffer buffer, final Date defaultValue)
    {

        TypeConstructor constructor = readConstructor(buffer);
        Object val = constructor.readValue(buffer);
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

    public Binary readBinary(ReadableBuffer buffer)
    {
        return readBinary(buffer, null);
    }

    public Binary readBinary(ReadableBuffer buffer, final Binary defaultValue)
    {

        TypeConstructor constructor = readConstructor(buffer);
        Object val = constructor.readValue(buffer);
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

    public Symbol readSymbol(ReadableBuffer buffer)
    {
        return readSymbol(buffer, null);
    }

    public Symbol readSymbol(ReadableBuffer buffer, final Symbol defaultValue)
    {

        TypeConstructor constructor = readConstructor(buffer);
        Object val = constructor.readValue(buffer);
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

    public String readString(ReadableBuffer buffer)
    {
        return readString(buffer, null);
    }

    public String readString(ReadableBuffer buffer, final String defaultValue)
    {

        TypeConstructor constructor = readConstructor(buffer);
        Object val = constructor.readValue(buffer);
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

    public List readList(ReadableBuffer buffer)
    {

        TypeConstructor constructor = readConstructor(buffer);
        Object val = constructor.readValue(buffer);
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

    public <T> void readList(ReadableBuffer buffer, final ListProcessor<T> processor)
    {
        //TODO.
    }

    public Map readMap(ReadableBuffer buffer)
    {

        TypeConstructor constructor = readConstructor(buffer);
        Object val = constructor.readValue(buffer);
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

    public <T> T[] readArray(ReadableBuffer buffer, final Class<T> clazz)
    {
        return null;  //TODO.
    }

    public Object[] readArray(ReadableBuffer buffer)
    {
        return (Object[]) readConstructor(buffer).readValue(buffer);

    }

    public boolean[] readBooleanArray(ReadableBuffer buffer)
    {
        return (boolean[]) ((ArrayType.ArrayEncoding)readConstructor(buffer)).readValueArray(buffer);
    }

    public byte[] readByteArray(ReadableBuffer buffer)
    {
        return (byte[]) ((ArrayType.ArrayEncoding)readConstructor(buffer)).readValueArray(buffer);
    }

    public short[] readShortArray(ReadableBuffer buffer)
    {
        return (short[]) ((ArrayType.ArrayEncoding)readConstructor(buffer)).readValueArray(buffer);
    }

    public int[] readIntegerArray(ReadableBuffer buffer)
    {
        return (int[]) ((ArrayType.ArrayEncoding)readConstructor(buffer)).readValueArray(buffer);
    }

    public long[] readLongArray(ReadableBuffer buffer)
    {
        return (long[]) ((ArrayType.ArrayEncoding)readConstructor(buffer)).readValueArray(buffer);
    }

    public float[] readFloatArray(ReadableBuffer buffer)
    {
        return (float[]) ((ArrayType.ArrayEncoding)readConstructor(buffer)).readValueArray(buffer);
    }

    public double[] readDoubleArray(ReadableBuffer buffer)
    {
        return (double[]) ((ArrayType.ArrayEncoding)readConstructor(buffer)).readValueArray(buffer);
    }

    public char[] readCharacterArray(ReadableBuffer buffer)
    {
        return (char[]) ((ArrayType.ArrayEncoding)readConstructor(buffer)).readValueArray(buffer);
    }

    public <T> T[] readMultiple(ReadableBuffer buffer, final Class<T> clazz)
    {
        Object val = readObject(buffer);
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

    public Object[] readMultiple(ReadableBuffer buffer)
    {
        Object val = readObject(buffer);
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

    public byte[] readByteMultiple(ReadableBuffer buffer)
    {
        return new byte[0];  //TODO.
    }

    public short[] readShortMultiple(ReadableBuffer buffer)
    {
        return new short[0];  //TODO.
    }

    public int[] readIntegerMultiple(ReadableBuffer buffer)
    {
        return new int[0];  //TODO.
    }

    public long[] readLongMultiple(ReadableBuffer buffer)
    {
        return new long[0];  //TODO.
    }

    public float[] readFloatMultiple(ReadableBuffer buffer)
    {
        return new float[0];  //TODO.
    }

    public double[] readDoubleMultiple(ReadableBuffer buffer)
    {
        return new double[0];  //TODO.
    }

    public char[] readCharacterMultiple(ReadableBuffer buffer)
    {
        return new char[0];  //TODO.
    }

    public Object readObject(ReadableBuffer buffer)
    {
        TypeConstructor constructor = readConstructor(buffer);
        if(constructor== null)
        {
            throw new DecodeException("Unknown constructor");
        }
        try
        {
            // TODO: if we called constructor.readValue(buffer) directly here, we would have the code running a lot faster
            //       Why?

            // Optimization: instanceof here would taken the double of the time
            if (constructor.isArray())
            {
                return ((ArrayType.ArrayEncoding)constructor).readValueArray(buffer);
            }
            else
            {
                return constructor.readValue(buffer);
            }

        }
        catch (NullPointerException npe)
        {
            throw new DecodeException("Unexpected null value - mandatory field not set? ("+npe.getMessage()+")", npe);
        }
        catch (ClassCastException cce)
        {
            throw new DecodeException("Incorrect type used", cce);
        }

    }

    public Object readObject(ReadableBuffer buffer, final Object defaultValue)
    {
        Object val = readObject(buffer);
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

    byte readRawByte(ReadableBuffer buffer)
    {
        return buffer.get();
    }

    int readRawInt(ReadableBuffer buffer)
    {
        return buffer.getInt();
    }

    long readRawLong(ReadableBuffer buffer)
    {
        return buffer.getLong();
    }

    short readRawShort(ReadableBuffer buffer)
    {
        return buffer.getShort();
    }

    float readRawFloat(ReadableBuffer buffer)
    {
        return buffer.getFloat();
    }

    double readRawDouble(ReadableBuffer buffer)
    {
        return buffer.getDouble();
    }

    void readRaw(ReadableBuffer buffer, final byte[] data, final int offset, final int length)
    {
        buffer.get(data, offset, length);
    }


    <V> V readRaw(ReadableBuffer buffer, TypeDecoder<V> decoder, int size)
    {
        V decode = decoder.decode(buffer.slice().limit(size));
        buffer.position(buffer.position()+size);
        return decode;
    }


    interface TypeDecoder<V>
    {
        V decode(ReadableBuffer buf);
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

    public int getByteBufferRemaining(ReadableBuffer buffer) {
        return buffer.remaining();
    }
}
