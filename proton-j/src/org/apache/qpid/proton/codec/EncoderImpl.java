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

import org.apache.qpid.proton.type.*;

import java.nio.ByteBuffer;
import java.util.*;

public final class EncoderImpl implements ByteBufferEncoder
{


    private static final byte DESCRIBED_TYPE_OP = (byte)0;

    public static interface Writable
    {
        void writeTo(ByteBuffer buf);
    }

    private ByteBuffer _buffer;

    private final Map<Class, AMQPType> _typeRegistry = new HashMap<Class, AMQPType>();
    private Map<Object, AMQPType> _describedDescriptorRegistry = new HashMap<Object, AMQPType>();
    private Map<Class, AMQPType>  _describedTypesClassRegistry = new HashMap<Class, AMQPType>();

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

    EncoderImpl(ByteBuffer buffer, DecoderImpl decoder)
    {
        this(decoder);
        setByteBuffer(buffer);
    }

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

    public void setByteBuffer(final ByteBuffer buf)
    {
        _buffer = buf;
    }

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

    public void writeNull()
    {
        _nullType.write();
    }

    public void writeBoolean(final boolean bool)
    {
        _booleanType.writeValue(bool);
    }

    public void writeBoolean(final Boolean bool)
    {
        if(bool == null)
        {
            writeNull();
        }
        else
        {
            _booleanType.write(bool);
        }
    }

    public void writeUnsignedByte(final UnsignedByte ubyte)
    {
        if(ubyte == null)
        {
            writeNull();
        }
        else
        {
            _unsignedByteType.write(ubyte);
        }
    }

    public void writeUnsignedShort(final UnsignedShort ushort)
    {
        if(ushort == null)
        {
            writeNull();
        }
        else
        {
            _unsignedShortType.write(ushort);
        }
    }

    public void writeUnsignedInteger(final UnsignedInteger uint)
    {
        if(uint == null)
        {
            writeNull();
        }
        else
        {
            _unsignedIntegerType.write(uint);
        }
    }

    public void writeUnsignedLong(final UnsignedLong ulong)
    {
        if(ulong == null)
        {
            writeNull();
        }
        else
        {
            _unsignedLongType.write(ulong);
        }
    }

    public void writeByte(final byte b)
    {
        _byteType.write(b);
    }

    public void writeByte(final Byte b)
    {
        if(b == null)
        {
            writeNull();
        }
        else
        {
            writeByte(b.byteValue());
        }
    }

    public void writeShort(final short s)
    {
        _shortType.write(s);
    }

    public void writeShort(final Short s)
    {
        if(s == null)
        {
            writeNull();
        }
        else
        {
            writeShort(s.shortValue());
        }
    }

    public void writeInteger(final int i)
    {
        _integerType.write(i);
    }

    public void writeInteger(final Integer i)
    {
        if(i == null)
        {
            writeNull();
        }
        else
        {
            writeInteger(i.intValue());
        }
    }

    public void writeLong(final long l)
    {
        _longType.write(l);
    }

    public void writeLong(final Long l)
    {

        if(l == null)
        {
            writeNull();
        }
        else
        {
            writeLong(l.longValue());
        }
    }

    public void writeFloat(final float f)
    {
        _floatType.write(f);
    }

    public void writeFloat(final Float f)
    {
        if(f == null)
        {
            writeNull();
        }
        else
        {
            writeFloat(f.floatValue());
        }
    }

    public void writeDouble(final double d)
    {
        _doubleType.write(d);
    }

    public void writeDouble(final Double d)
    {
        if(d == null)
        {
            writeNull();
        }
        else
        {
            writeDouble(d.doubleValue());
        }
    }

    public void writeDecimal32(final Decimal32 d)
    {
        if(d == null)
        {
            writeNull();
        }
        else
        {
            _decimal32Type.write(d);
        }
    }

    public void writeDecimal64(final Decimal64 d)
    {
        if(d == null)
        {
            writeNull();
        }
        else
        {
            _decimal64Type.write(d);
        }
    }

    public void writeDecimal128(final Decimal128 d)
    {
        if(d == null)
        {
            writeNull();
        }
        else
        {
            _decimal128Type.write(d);
        }
    }

    public void writeCharacter(final char c)
    {
        // TODO - java character may be half of a pair, should probably throw exception then
        _characterType.write(c);
    }

    public void writeCharacter(final Character c)
    {
        if(c == null)
        {
            writeNull();
        }
        else
        {
            writeCharacter(c.charValue());
        }
    }

    public void writeTimestamp(final long d)
    {
        _timestampType.write(d);
    }

    public void writeTimestamp(final Date d)
    {
        if(d == null)
        {
            writeNull();
        }
        else
        {
            writeTimestamp(d.getTime());
        }
    }

    public void writeUUID(final UUID uuid)
    {
        if(uuid == null)
        {
            writeNull();
        }
        else
        {
            _uuidType.write(uuid);
        }

    }

    public void writeBinary(final Binary b)
    {
        if(b == null)
        {
            writeNull();
        }
        else
        {
            _binaryType.write(b);
        }
    }

    public void writeString(final String s)
    {
        if(s == null)
        {
            writeNull();
        }
        else
        {
            _stringType.write(s);
        }
    }

    public void writeSymbol(final Symbol s)
    {
        if(s == null)
        {
            writeNull();
        }
        else
        {
            _symbolType.write(s);
        }

    }

    public void writeList(final List l)
    {
        if(l == null)
        {
            writeNull();
        }
        else
        {
            _listType.write(l);
        }
    }

    public void writeMap(final Map m)
    {

        if(m == null)
        {
            writeNull();
        }
        else
        {
            _mapType.write(m);
        }
    }

    public void writeDescribedType(final DescribedType d)
    {
        if(d == null)
        {
            writeNull();
        }
        else
        {
            _buffer.put(DESCRIBED_TYPE_OP);
            writeObject(d.getDescriptor());
            writeObject(d.getDescribed());
        }
    }

    public void writeArray(final boolean[] a)
    {
        if(a == null)
        {
            writeNull();
        }
        else
        {
            _arrayType.write(a);
        }
    }

    public void writeArray(final byte[] a)
    {
        if(a == null)
        {
            writeNull();
        }
        else
        {
            _arrayType.write(a);
        }
    }

    public void writeArray(final short[] a)
    {
        if(a == null)
        {
            writeNull();
        }
        else
        {
            _arrayType.write(a);
        }
    }

    public void writeArray(final int[] a)
    {
        if(a == null)
        {
            writeNull();
        }
        else
        {
            _arrayType.write(a);
        }
    }

    public void writeArray(final long[] a)
    {
        if(a == null)
        {
            writeNull();
        }
        else
        {
            _arrayType.write(a);
        }
    }

    public void writeArray(final float[] a)
    {
        if(a == null)
        {
            writeNull();
        }
        else
        {
            _arrayType.write(a);
        }
    }

    public void writeArray(final double[] a)
    {
        if(a == null)
        {
            writeNull();
        }
        else
        {
            _arrayType.write(a);
        }
    }

    public void writeArray(final char[] a)
    {
        if(a == null)
        {
            writeNull();
        }
        else
        {
            _arrayType.write(a);
        }
    }

    public void writeArray(final Object[] a)
    {
        if(a == null)
        {
            writeNull();
        }
        else
        {
            _arrayType.write(a);
        }
    }

    public void writeObject(final Object o)
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
                        writeArray((boolean[])o);
                    }
                    else if(componentType == Byte.TYPE)
                    {
                        writeArray((byte[])o);
                    }
                    else if(componentType == Short.TYPE)
                    {
                        writeArray((short[])o);
                    }
                    else if(componentType == Integer.TYPE)
                    {
                        writeArray((int[])o);
                    }
                    else if(componentType == Long.TYPE)
                    {
                        writeArray((long[])o);
                    }
                    else if(componentType == Float.TYPE)
                    {
                        writeArray((float[])o);
                    }
                    else if(componentType == Double.TYPE)
                    {
                        writeArray((double[])o);
                    }
                    else if(componentType == Character.TYPE)
                    {
                        writeArray((char[])o);
                    }
                    else
                    {
                        throw new IllegalArgumentException("Cannot write arrays of type " + componentType.getName());
                    }
                }
                else
                {
                    writeArray((Object[]) o);
                }
            }
            else if(o instanceof List)
            {
                writeList((List)o);
            }
            else if(o instanceof Map)
            {
                writeMap((Map)o);
            }
            else if(o instanceof DescribedType)
            {
                writeDescribedType((DescribedType)o);
            }
            else
            {
                throw new IllegalArgumentException("Do not know how to write Objects of class " + o.getClass()
                                                                                                       .getName());

            }
        }
        else
        {
            type.write(o);
        }
    }

    void writeRaw(final byte b)
    {
        _buffer.put(b);
    }

    void writeRaw(final short s)
    {
        _buffer.putShort(s);
    }

    void writeRaw(final int i)
    {
        _buffer.putInt(i);
    }

    void writeRaw(final long l)
    {
        _buffer.putLong(l);
    }

    void writeRaw(final float f)
    {
        _buffer.putFloat(f);
    }

    void writeRaw(final double d)
    {
        _buffer.putDouble(d);
    }

    void writeRaw(byte[] src, int offset, int length)
    {
        _buffer.put(src, offset, length);
    }

    void writeRaw(Writable writable)
    {
        writable.writeTo(_buffer);
    }



    public static class ExampleObject implements DescribedType
    {
        public static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(254l);

        private int _handle;
        private String _name;

        private final ExampleObjectWrapper _wrapper = new ExampleObjectWrapper();

        public ExampleObject()
        {

        }

        public ExampleObject(final int handle, final String name)
        {
            _handle = handle;
            _name = name;

        }

        public int getHandle()
        {
            return _handle;
        }

        public String getName()
        {
            return _name;
        }

        public void setHandle(final int handle)
        {
            _handle = handle;
        }

        public void setName(final String name)
        {
            _name = name;
        }

        public Object getDescriptor()
        {
            return DESCRIPTOR;
        }

        public Object getDescribed()
        {
            return _wrapper;
        }

        private class ExampleObjectWrapper extends AbstractList
        {

            @Override
            public Object get(final int index)
            {
                switch(index)
                {
                    case 0:
                        return getHandle();
                    case 1:
                        return getName();
                }
                throw new IllegalStateException("Unknown index " + index);
            }

            @Override
            public int size()
            {
                return getName() == null ? getHandle() == 0 ? 0 : 1 : 2;
            }
        }

    }


    public static void main(String[] args)
    {
        byte[] data = new byte[1024];

        DecoderImpl decoder = new DecoderImpl(ByteBuffer.wrap(data));
        Encoder enc = new EncoderImpl(ByteBuffer.wrap(data), decoder);

        decoder.register(ExampleObject.DESCRIPTOR, new DescribedTypeConstructor<ExampleObject>()
        {
            public ExampleObject newInstance(final Object described)
            {
                List l = (List) described;
                ExampleObject o = new ExampleObject();
                switch(l.size())
                {
                    case 2:
                        o.setName((String)l.get(1));
                    case 1:
                        o.setHandle((Integer)l.get(0));
                }
                return o;
            }

            public Class<ExampleObject> getTypeClass()
            {
                return ExampleObject.class;
            }
        }
        );

        ExampleObject[] examples = new ExampleObject[2];
        examples[0] = new ExampleObject(7,"fred");
        examples[1] = new ExampleObject(19, "bret");

        enc.writeObject(examples);

        enc.writeObject(new Object[][]{new Integer[]{1,2,3}, new Long[]{4l,5L,6L}, new String[] {"hello", "world"}});
        enc.writeObject(new int[][]{{256,27}, {1}});

        Object rval = decoder.readObject();
        rval = decoder.readObject();

        System.out.println("Hello");


    }


}
