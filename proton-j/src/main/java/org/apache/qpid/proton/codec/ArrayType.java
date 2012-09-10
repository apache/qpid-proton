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

import java.lang.reflect.Array;
import java.util.Arrays;
import java.util.Collection;

public class ArrayType implements PrimitiveType<Object[]>
{
    private final EncoderImpl _encoder;
    private final BooleanType _booleanType;
    private final ByteType _byteType;
    private final ShortType _shortType;
    private final IntegerType _integerType;
    private final LongType _longType;
    private final FloatType _floatType;
    private final DoubleType _doubleType;
    private final CharacterType _characterType;

    public static interface ArrayEncoding extends PrimitiveTypeEncoding<Object[]>
    {
        void writeValue(boolean[] a);
        void writeValue(byte[] a);
        void writeValue(short[] a);
        void writeValue(int[] a);
        void writeValue(long[] a);
        void writeValue(float[] a);
        void writeValue(double[] a);
        void writeValue(char[] a);

        void setValue(Object[] val, TypeEncoding encoder, int size);

        int getSizeBytes();

        Object readValueArray();
    }

    private final ArrayEncoding _shortArrayEncoding;
    private final ArrayEncoding _arrayEncoding;

    public ArrayType(EncoderImpl encoder,
                     final DecoderImpl decoder, BooleanType boolType,
                     ByteType byteType,
                     ShortType shortType,
                     IntegerType intType,
                     LongType longType,
                     FloatType floatType,
                     DoubleType doubleType,
                     CharacterType characterType)
    {
        _encoder = encoder;
        _booleanType = boolType;
        _byteType = byteType;
        _shortType = shortType;
        _integerType = intType;
        _longType = longType;
        _floatType = floatType;
        _doubleType = doubleType;
        _characterType = characterType;

        _arrayEncoding = new AllArrayEncoding(encoder, decoder);
        _shortArrayEncoding = new ShortArrayEncoding(encoder, decoder);

        encoder.register(Object[].class, this);
        decoder.register(this);
    }

    public Class<Object[]> getTypeClass()
    {
        return Object[].class;
    }

    public ArrayEncoding getEncoding(final Object[] val)
    {
        TypeEncoding encoder = calculateEncoder(val,_encoder);
        int size = calculateSize(val, encoder);
        ArrayEncoding arrayEncoding = (val.length > 255 || size > 254)
                                      ? _arrayEncoding
                                      : _shortArrayEncoding;
        arrayEncoding.setValue(val, encoder, size);
        return arrayEncoding;
    }

    private static TypeEncoding calculateEncoder(final Object[] val, final EncoderImpl encoder)
    {

        if(val.length == 0)
        {
            AMQPType underlyingType = encoder.getTypeFromClass(val.getClass().getComponentType());
            return underlyingType.getCanonicalEncoding();
        }
        else
        {
            AMQPType underlyingType = encoder.getTypeFromClass(val.getClass().getComponentType());
            boolean checkTypes = false;

            if(val[0].getClass().isArray() && val[0].getClass().getComponentType().isPrimitive())
            {
                Class componentType = val[0].getClass().getComponentType();
                if(componentType == Boolean.TYPE)
                {
                    return ((ArrayType)underlyingType).getEncoding((boolean[])val[0]);
                }
                else if(componentType == Byte.TYPE)
                {
                    return ((ArrayType)underlyingType).getEncoding((byte[])val[0]);
                }
                else if(componentType == Short.TYPE)
                {
                    return ((ArrayType)underlyingType).getEncoding((short[])val[0]);
                }
                else if(componentType == Integer.TYPE)
                {
                    return ((ArrayType)underlyingType).getEncoding((int[])val[0]);
                }
                else if(componentType == Long.TYPE)
                {
                    return ((ArrayType)underlyingType).getEncoding((long[])val[0]);
                }
                else if(componentType == Float.TYPE)
                {
                    return ((ArrayType)underlyingType).getEncoding((float[])val[0]);
                }
                else if(componentType == Double.TYPE)
                {
                    return ((ArrayType)underlyingType).getEncoding((double[])val[0]);
                }
                else if(componentType == Character.TYPE)
                {
                    return ((ArrayType)underlyingType).getEncoding((char[])val[0]);
                }
                else
                {
                    throw new IllegalArgumentException("Cannot encode arrays of type " + componentType.getName());
                }
            }
            else
            {

                if(underlyingType == null)
                {
                    checkTypes = true;
                    underlyingType = encoder.getType(val[0]);
                }
                TypeEncoding underlyingEncoding = underlyingType.getEncoding(val[0]);
                TypeEncoding canonicalEncoding = underlyingType.getCanonicalEncoding();

                for(int i = 0; i < val.length && (checkTypes || underlyingEncoding != canonicalEncoding); i++)
                {
                    if(checkTypes && encoder.getType(val[i]) != underlyingType)
                    {
                        throw new IllegalArgumentException("Non matching types " + underlyingType + " and " + encoder
                                .getType(val[i]) + " in array");
                    }


                    TypeEncoding elementEncoding = underlyingType.getEncoding(val[i]);
                    if(elementEncoding != underlyingEncoding && !underlyingEncoding.encodesSuperset(elementEncoding))
                    {
                        if(elementEncoding.encodesSuperset(underlyingEncoding))
                        {
                            underlyingEncoding = elementEncoding;
                        }
                        else
                        {
                            underlyingEncoding = canonicalEncoding;
                        }
                    }

                }

                return underlyingEncoding;
            }
        }
    }

    private static int calculateSize(final Object[] val, final TypeEncoding encoder)
    {
        int size = encoder.getConstructorSize();
        if(encoder.isFixedSizeVal())
        {
            size += val.length * encoder.getValueSize(null);
        }
        else
        {
            for(Object o : val)
            {
                if(o.getClass().isArray() && o.getClass().getComponentType().isPrimitive())
                {
                    ArrayEncoding arrayEncoding = (ArrayEncoding) encoder;
                    ArrayType arrayType = (ArrayType) arrayEncoding.getType();

                    Class componentType = o.getClass().getComponentType();

                    size += 2 * arrayEncoding.getSizeBytes();

                    TypeEncoding componentEncoding;
                    int componentCount;

                    if(componentType == Boolean.TYPE)
                    {
                        boolean[] componentArray = (boolean[]) o;
                        componentEncoding = arrayType.getUnderlyingEncoding(componentArray);
                        componentCount = componentArray.length;
                    }
                    else if(componentType == Byte.TYPE)
                    {
                        byte[] componentArray = (byte[]) o;
                        componentEncoding = arrayType.getUnderlyingEncoding(componentArray);
                        componentCount = componentArray.length;
                    }
                    else if(componentType == Short.TYPE)
                    {
                        short[] componentArray = (short[]) o;
                        componentEncoding = arrayType.getUnderlyingEncoding(componentArray);
                        componentCount = componentArray.length;
                    }
                    else if(componentType == Integer.TYPE)
                    {
                        int[] componentArray = (int[]) o;
                        componentEncoding = arrayType.getUnderlyingEncoding(componentArray);
                        componentCount = componentArray.length;
                    }
                    else if(componentType == Long.TYPE)
                    {
                        long[] componentArray = (long[]) o;
                        componentEncoding = arrayType.getUnderlyingEncoding(componentArray);
                        componentCount = componentArray.length;
                    }
                    else if(componentType == Float.TYPE)
                    {
                        float[] componentArray = (float[]) o;
                        componentEncoding = arrayType.getUnderlyingEncoding(componentArray);
                        componentCount = componentArray.length;
                    }
                    else if(componentType == Double.TYPE)
                    {
                        double[] componentArray = (double[]) o;
                        componentEncoding = arrayType.getUnderlyingEncoding(componentArray);
                        componentCount = componentArray.length;
                    }
                    else if(componentType == Character.TYPE)
                    {
                        char[] componentArray = (char[]) o;
                        componentEncoding = arrayType.getUnderlyingEncoding(componentArray);
                        componentCount = componentArray.length;
                    }
                    else
                    {
                        throw new IllegalArgumentException("Cannot encode arrays of type " + componentType.getName());
                    }

                    size +=  componentEncoding.getConstructorSize()
                                + componentEncoding.getValueSize(null) * componentCount;

                }
                else
                {
                    size += encoder.getValueSize(o);
                }
            }
        }

        return size;
    }

    public ArrayEncoding getCanonicalEncoding()
    {
        return _arrayEncoding;
    }

    public Collection<ArrayEncoding> getAllEncodings()
    {
        return Arrays.asList(_shortArrayEncoding, _arrayEncoding);
    }

    public void write(final Object[] val)
    {
        ArrayEncoding encoding = getEncoding(val);
        encoding.writeConstructor();
        encoding.writeValue(val);
    }

    public void write(boolean[] a)
    {
        ArrayEncoding encoding = getEncoding(a);
        encoding.writeConstructor();
        encoding.writeValue(a);
    }

    private ArrayEncoding getEncoding(final boolean[] a)
    {
        return a.length < 254 || a.length <= 255 && allSameValue(a) ? _shortArrayEncoding : _arrayEncoding;
    }

    private boolean allSameValue(final boolean[] a)
    {
        boolean val = a[0];
        for(int i = 1; i < a.length; i++)
        {
            if(val != a[i])
            {
                return false;
            }
        }
        return true;
    }

    public void write(byte[] a)
    {
        ArrayEncoding encoding = getEncoding(a);
        encoding.writeConstructor();
        encoding.writeValue(a);
    }

    private ArrayEncoding getEncoding(final byte[] a)
    {
        return a.length < 254 ? _shortArrayEncoding : _arrayEncoding;
    }

    public void write(short[] a)
    {
        ArrayEncoding encoding = getEncoding(a);
        encoding.writeConstructor();
        encoding.writeValue(a);
    }

    private ArrayEncoding getEncoding(final short[] a)
    {
        return a.length < 127 ? _shortArrayEncoding : _arrayEncoding;
    }

    public void write(int[] a)
    {
        ArrayEncoding encoding = getEncoding(a);
        encoding.writeConstructor();
        encoding.writeValue(a);
    }

    private ArrayEncoding getEncoding(final int[] a)
    {
        return a.length < 63 || (a.length < 254 && allSmallInts(a)) ? _shortArrayEncoding : _arrayEncoding;
    }

    private boolean allSmallInts(final int[] a)
    {
        for(int i = 0; i < a.length; i++)
        {
            if(a[i] < -128 || a[i] > 127)
            {
                return false;
            }
        }
        return true;
    }

    public void write(long[] a)
    {
        ArrayEncoding encoding = getEncoding(a);
        encoding.writeConstructor();
        encoding.writeValue(a);
    }

    private ArrayEncoding getEncoding(final long[] a)
    {
        return a.length < 31 || (a.length < 254 && allSmallLongs(a)) ? _shortArrayEncoding : _arrayEncoding;
    }

    private boolean allSmallLongs(final long[] a)
    {
        for(int i = 0; i < a.length; i++)
        {
            if(a[i] < -128L || a[i] > 127L)
            {
                return false;
            }
        }
        return true;
    }

    public void write(float[] a)
    {
        ArrayEncoding encoding = getEncoding(a);
        encoding.writeConstructor();
        encoding.writeValue(a);
    }

    private ArrayEncoding getEncoding(final float[] a)
    {
        return a.length < 63 ? _shortArrayEncoding : _arrayEncoding;
    }

    public void write(double[] a)
    {
        ArrayEncoding encoding = getEncoding(a);
        encoding.writeConstructor();
        encoding.writeValue(a);
    }

    private ArrayEncoding getEncoding(final double[] a)
    {
        return a.length < 31 ? _shortArrayEncoding : _arrayEncoding;
    }

    public void write(char[] a)
    {
        ArrayEncoding encoding = getEncoding(a);
        encoding.writeConstructor();
        encoding.writeValue(a);
    }

    private ArrayEncoding getEncoding(final char[] a)
    {
        return a.length < 63 ? _shortArrayEncoding : _arrayEncoding;
    }

    private class AllArrayEncoding
            extends LargeFloatingSizePrimitiveTypeEncoding<Object[]>
            implements ArrayEncoding
    {

        private Object[] _val;
        private TypeEncoding _underlyingEncoder;
        private int _size;

        AllArrayEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
        {
            super(encoder, decoder);
        }

        public void writeValue(final boolean[] a)
        {
            BooleanType.BooleanEncoding underlyingEncoder = getUnderlyingEncoding(a);
            getEncoder().writeRaw(4 + underlyingEncoder.getConstructorSize()
                                  + a.length*underlyingEncoder.getValueSize(null));
            getEncoder().writeRaw(a.length);
            underlyingEncoder.writeConstructor();
            for(boolean b : a)
            {
                underlyingEncoder.writeValue(b);
            }

        }

        public void writeValue(final byte[] a)
        {
            ByteType.ByteEncoding underlyingEncoder = getUnderlyingEncoding(a);
            getEncoder().writeRaw(4 + underlyingEncoder.getConstructorSize()
                                  + a.length*underlyingEncoder.getValueSize(null));
            getEncoder().writeRaw(a.length);
            underlyingEncoder.writeConstructor();
            for(byte b : a)
            {
                underlyingEncoder.writeValue(b);
            }
        }

        public void writeValue(final short[] a)
        {
            ShortType.ShortEncoding underlyingEncoder = getUnderlyingEncoding(a);
            getEncoder().writeRaw(4 + underlyingEncoder.getConstructorSize()
                                  + a.length*underlyingEncoder.getValueSize(null));
            getEncoder().writeRaw(a.length);
            underlyingEncoder.writeConstructor();
            for(short b : a)
            {
                underlyingEncoder.writeValue(b);
            }
        }

        public void writeValue(final int[] a)
        {

            IntegerType.IntegerEncoding underlyingEncoder = getUnderlyingEncoding(a);
            getEncoder().writeRaw(4 + underlyingEncoder.getConstructorSize()
                                  + a.length*underlyingEncoder.getValueSize(null));
            getEncoder().writeRaw(a.length);
            underlyingEncoder.writeConstructor();
            for(int b : a)
            {
                underlyingEncoder.writeValue(b);
            }
        }

        public void writeValue(final long[] a)
        {

            LongType.LongEncoding underlyingEncoder = getUnderlyingEncoding(a);
            getEncoder().writeRaw(4 + underlyingEncoder.getConstructorSize()
                                  + a.length*underlyingEncoder.getValueSize(null));
            getEncoder().writeRaw(a.length);
            underlyingEncoder.writeConstructor();
            for(long b : a)
            {
                underlyingEncoder.writeValue(b);
            }
        }

        public void writeValue(final float[] a)
        {

            FloatType.FloatEncoding underlyingEncoder = getUnderlyingEncoding(a);
            getEncoder().writeRaw(4 + underlyingEncoder.getConstructorSize()
                                  + a.length*underlyingEncoder.getValueSize(null));
            getEncoder().writeRaw(a.length);
            underlyingEncoder.writeConstructor();
            for(float b : a)
            {
                underlyingEncoder.writeValue(b);
            }
        }

        public void writeValue(final double[] a)
        {

            DoubleType.DoubleEncoding underlyingEncoder = getUnderlyingEncoding(a);
            getEncoder().writeRaw(4 + underlyingEncoder.getConstructorSize()
                                  + a.length*underlyingEncoder.getValueSize(null));
            getEncoder().writeRaw(a.length);
            underlyingEncoder.writeConstructor();
            for(double b : a)
            {
                underlyingEncoder.writeValue(b);
            }
        }

        public void writeValue(final char[] a)
        {

            CharacterType.CharacterEncoding underlyingEncoder = getUnderlyingEncoding(a);
            getEncoder().writeRaw(4 + underlyingEncoder.getConstructorSize()
                                  + a.length*underlyingEncoder.getValueSize(null));
            getEncoder().writeRaw(a.length);
            underlyingEncoder.writeConstructor();
            for(char b : a)
            {
                underlyingEncoder.writeValue(b);
            }
        }

        public void setValue(final Object[] val, final TypeEncoding encoder, final int size)
        {
            _val = val;
            _underlyingEncoder = encoder;
            _size = size;
        }

        @Override
        protected void writeEncodedValue(final Object[] val)
        {
            TypeEncoding underlyingEncoder;

            if(_val != val)
            {
                _val = val;
                _underlyingEncoder = underlyingEncoder = calculateEncoder(val, getEncoder());
                _size =  calculateSize(val, underlyingEncoder);
            }
            else
            {
                underlyingEncoder = _underlyingEncoder;
            }
            getEncoder().writeRaw(val.length);
            underlyingEncoder.writeConstructor();
            for(Object o : val)
            {
                underlyingEncoder.writeValue(o);
            }
        }

        @Override
        protected int getEncodedValueSize(final Object[] val)
        {
            if(_val != val)
            {
                _val = val;
                _underlyingEncoder = calculateEncoder(val, getEncoder());
                _size = calculateSize(val, _underlyingEncoder);
            }
            return 4 + _size;
        }

        @Override
        public byte getEncodingCode()
        {
            return EncodingCodes.ARRAY32;
        }

        public ArrayType getType()
        {
            return ArrayType.this;
        }

        public boolean encodesSuperset(final TypeEncoding<Object[]> encoding)
        {
            return getType() == encoding.getType();
        }

        public Object[] readValue()
        {
            DecoderImpl decoder = getDecoder();
            int size = decoder.readRawInt();
            int count = decoder.readRawInt();
            return decodeArray(decoder, count);
        }

        public Object readValueArray()
        {
            DecoderImpl decoder = getDecoder();
            int size = decoder.readRawInt();
            int count = decoder.readRawInt();
            return decodeArrayAsObject(decoder, count);
        }



    }



    private class ShortArrayEncoding
            extends SmallFloatingSizePrimitiveTypeEncoding<Object[]>
            implements ArrayEncoding
    {

        private Object[] _val;
        private TypeEncoding _underlyingEncoder;
        private int _size;

        ShortArrayEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
        {
            super(encoder, decoder);
        }

        public void writeValue(final boolean[] a)
        {
            BooleanType.BooleanEncoding underlyingEncoder = getUnderlyingEncoding(a);
            getEncoder().writeRaw((byte)(1 + underlyingEncoder.getConstructorSize()
                                  + a.length*underlyingEncoder.getValueSize(null)));
            getEncoder().writeRaw((byte)a.length);
            underlyingEncoder.writeConstructor();
            for(boolean b : a)
            {
                underlyingEncoder.writeValue(b);
            }

        }

        public void writeValue(final byte[] a)
        {
            ByteType.ByteEncoding underlyingEncoder = getUnderlyingEncoding(a);
            getEncoder().writeRaw((byte)(1 + underlyingEncoder.getConstructorSize()
                                  + a.length*underlyingEncoder.getValueSize(null)));
            getEncoder().writeRaw((byte)a.length);
            underlyingEncoder.writeConstructor();
            for(byte b : a)
            {
                underlyingEncoder.writeValue(b);
            }
        }

        public void writeValue(final short[] a)
        {
            ShortType.ShortEncoding underlyingEncoder = getUnderlyingEncoding(a);
            getEncoder().writeRaw((byte)(1 + underlyingEncoder.getConstructorSize()
                                  + a.length*underlyingEncoder.getValueSize(null)));
            getEncoder().writeRaw((byte)a.length);
            underlyingEncoder.writeConstructor();
            for(short b : a)
            {
                underlyingEncoder.writeValue(b);
            }
        }

        public void writeValue(final int[] a)
        {

            IntegerType.IntegerEncoding underlyingEncoder = getUnderlyingEncoding(a);
            getEncoder().writeRaw((byte)(1 + underlyingEncoder.getConstructorSize()
                                  + a.length*underlyingEncoder.getValueSize(null)));
            getEncoder().writeRaw((byte)a.length);
            underlyingEncoder.writeConstructor();
            for(int b : a)
            {
                underlyingEncoder.writeValue(b);
            }
        }

        public void writeValue(final long[] a)
        {

            LongType.LongEncoding underlyingEncoder = getUnderlyingEncoding(a);
            getEncoder().writeRaw((byte)(1 + underlyingEncoder.getConstructorSize()
                                  + a.length*underlyingEncoder.getValueSize(null)));
            getEncoder().writeRaw((byte)a.length);
            underlyingEncoder.writeConstructor();
            for(long b : a)
            {
                underlyingEncoder.writeValue(b);
            }
        }

        public void writeValue(final float[] a)
        {

            FloatType.FloatEncoding underlyingEncoder = getUnderlyingEncoding(a);
            getEncoder().writeRaw((byte)(1 + underlyingEncoder.getConstructorSize()
                                  + a.length*underlyingEncoder.getValueSize(null)));
            getEncoder().writeRaw((byte)a.length);
            underlyingEncoder.writeConstructor();
            for(float b : a)
            {
                underlyingEncoder.writeValue(b);
            }
        }

        public void writeValue(final double[] a)
        {

            DoubleType.DoubleEncoding underlyingEncoder = getUnderlyingEncoding(a);
            getEncoder().writeRaw((byte)(1 + underlyingEncoder.getConstructorSize()
                                  + a.length*underlyingEncoder.getValueSize(null)));
            getEncoder().writeRaw((byte)a.length);
            underlyingEncoder.writeConstructor();
            for(double b : a)
            {
                underlyingEncoder.writeValue(b);
            }
        }

        public void writeValue(final char[] a)
        {

            CharacterType.CharacterEncoding underlyingEncoder = getUnderlyingEncoding(a);
            getEncoder().writeRaw((byte)(1 + underlyingEncoder.getConstructorSize()
                                  + a.length*underlyingEncoder.getValueSize(null)));
            getEncoder().writeRaw((byte)a.length);
            underlyingEncoder.writeConstructor();
            for(char b : a)
            {
                underlyingEncoder.writeValue(b);
            }
        }

        public void setValue(final Object[] val, final TypeEncoding encoder, final int size)
        {
            _val = val;
            _underlyingEncoder = encoder;
            _size = size;
        }

        @Override
        protected void writeEncodedValue(final Object[] val)
        {
            TypeEncoding underlyingEncoder;

            if(_val != val)
            {
                _val = val;
                _underlyingEncoder = underlyingEncoder = calculateEncoder(val, getEncoder());
                _size =  calculateSize(val, underlyingEncoder);
            }
            else
            {
                underlyingEncoder = _underlyingEncoder;
            }
            getEncoder().writeRaw((byte)val.length);
            underlyingEncoder.writeConstructor();
            for(Object o : val)
            {
                if(o.getClass().isArray() && o.getClass().getComponentType().isPrimitive())
                {
                    ArrayEncoding arrayEncoding = (ArrayEncoding) underlyingEncoder;
                    ArrayType arrayType = (ArrayType) arrayEncoding.getType();

                    Class componentType = o.getClass().getComponentType();

                    if(componentType == Boolean.TYPE)
                    {
                        boolean[] componentArray = (boolean[]) o;
                        arrayEncoding.writeValue(componentArray);
                    }
                    else if(componentType == Byte.TYPE)
                    {
                        byte[] componentArray = (byte[]) o;
                        arrayEncoding.writeValue(componentArray);
                    }
                    else if(componentType == Short.TYPE)
                    {
                        short[] componentArray = (short[]) o;
                        arrayEncoding.writeValue(componentArray);
                    }
                    else if(componentType == Integer.TYPE)
                    {
                        int[] componentArray = (int[]) o;
                        arrayEncoding.writeValue(componentArray);
                    }
                    else if(componentType == Long.TYPE)
                    {
                        long[] componentArray = (long[]) o;
                        arrayEncoding.writeValue(componentArray);
                    }
                    else if(componentType == Float.TYPE)
                    {
                        float[] componentArray = (float[]) o;
                        arrayEncoding.writeValue(componentArray);
                    }
                    else if(componentType == Double.TYPE)
                    {
                        double[] componentArray = (double[]) o;
                        arrayEncoding.writeValue(componentArray);
                    }
                    else if(componentType == Character.TYPE)
                    {
                        char[] componentArray = (char[]) o;
                        arrayEncoding.writeValue(componentArray);
                    }
                    else
                    {
                        throw new IllegalArgumentException("Cannot encode arrays of type " + componentType.getName());
                    }

                }
                else
                {
                    underlyingEncoder.writeValue(o);
                }
            }
        }

        @Override
        protected int getEncodedValueSize(final Object[] val)
        {
            if(_val != val)
            {
                _val = val;
                _underlyingEncoder = calculateEncoder(val, getEncoder());
                _size = calculateSize(val, _underlyingEncoder);
            }
            return 1 + _size;
        }

        @Override
        public byte getEncodingCode()
        {
            return EncodingCodes.ARRAY8;
        }

        public ArrayType getType()
        {
            return ArrayType.this;
        }

        public boolean encodesSuperset(final TypeEncoding<Object[]> encoding)
        {
            return getType() == encoding.getType();
        }

        public Object[] readValue()
        {
            DecoderImpl decoder = getDecoder();
            int size = ((int)decoder.readRawByte()) & 0xFF;
            int count = ((int)decoder.readRawByte()) & 0xFF;
            return decodeArray(decoder, count);
        }

        public Object readValueArray()
        {
            DecoderImpl decoder = getDecoder();
            int size = ((int)decoder.readRawByte()) & 0xFF;
            int count = ((int)decoder.readRawByte()) & 0xFF;
            return decodeArrayAsObject(decoder, count);
        }

    }

    private BooleanType.BooleanEncoding getUnderlyingEncoding(final boolean[] a)
    {
        if(a.length == 0)
        {
            return _booleanType.getCanonicalEncoding();
        }
        else
        {
            boolean val = a[0];
            for(int i = 1; i < a.length; i++)
            {
                if(val != a[i])
                {
                    return _booleanType.getCanonicalEncoding();
                }
            }
            return _booleanType.getEncoding(val);
        }
    }


    private ByteType.ByteEncoding getUnderlyingEncoding(final byte[] a)
    {
        return _byteType.getCanonicalEncoding();
    }


    private ShortType.ShortEncoding getUnderlyingEncoding(final short[] a)
    {
        return _shortType.getCanonicalEncoding();
    }

    private IntegerType.IntegerEncoding getUnderlyingEncoding(final int[] a)
    {
        if(a.length == 0 || !allSmallInts(a))
        {
            return _integerType.getCanonicalEncoding();
        }
        else
        {
            return _integerType.getEncoding(a[0]);
        }
    }

    private LongType.LongEncoding getUnderlyingEncoding(final long[] a)
    {
        if(a.length == 0 || !allSmallLongs(a))
        {
            return _longType.getCanonicalEncoding();
        }
        else
        {
            return _longType.getEncoding(a[0]);
        }
    }


    private FloatType.FloatEncoding getUnderlyingEncoding(final float[] a)
    {
        return _floatType.getCanonicalEncoding();
    }


    private DoubleType.DoubleEncoding getUnderlyingEncoding(final double[] a)
    {
        return _doubleType.getCanonicalEncoding();
    }


    private CharacterType.CharacterEncoding getUnderlyingEncoding(final char[] a)
    {
        return _characterType.getCanonicalEncoding();
    }

    private static Object[] decodeArray(final DecoderImpl decoder, final int count)
    {
        TypeConstructor constructor = decoder.readConstructor();
        return decodeNonPrimitive(constructor, count);
    }

    private static Object[] decodeNonPrimitive(final TypeConstructor constructor,
                                               final int count)
    {
        if(constructor instanceof ArrayEncoding)
        {
            ArrayEncoding arrayEncoding = (ArrayEncoding) constructor;

            Object[] array = new Object[count];
            for(int i = 0; i < count; i++)
            {
                array[i] = arrayEncoding.readValueArray();
            }

            return array;
        }
        else
        {
            Object[] array = (Object[]) Array.newInstance(constructor.getTypeClass(), count);

            for(int i = 0; i < count; i++)
            {
                array[i] = constructor.readValue();
            }

            return array;
        }
    }

    private static Object decodeArrayAsObject(final DecoderImpl decoder, final int count)
    {
        TypeConstructor constructor = decoder.readConstructor();
        if(constructor.encodesJavaPrimitive())
        {
            if(constructor instanceof BooleanType.BooleanEncoding)
            {
                return decodeBooleanArray((BooleanType.BooleanEncoding) constructor, count);
            }
            else if(constructor instanceof ByteType.ByteEncoding)
            {
                return decodeByteArray((ByteType.ByteEncoding)constructor, count);
            }
            else if(constructor instanceof ShortType.ShortEncoding)
            {
                return decodeShortArray((ShortType.ShortEncoding)constructor, count);
            }
            else if(constructor instanceof IntegerType.IntegerEncoding)
            {
                return decodeIntArray((IntegerType.IntegerEncoding)constructor, count);
            }
            else if(constructor instanceof LongType.LongEncoding)
            {
                return decodeLongArray((LongType.LongEncoding) constructor, count);
            }
            else if(constructor instanceof FloatType.FloatEncoding)
            {
                return decodeFloatArray((FloatType.FloatEncoding) constructor, count);
            }
            else if(constructor instanceof DoubleType.DoubleEncoding)
            {
                return decodeDoubleArray((DoubleType.DoubleEncoding)constructor, count);
            }
            else
            {
                throw new ClassCastException("Unexpected class " + constructor.getClass().getName());
            }

        }
        else
        {
            return decodeNonPrimitive(constructor, count);
        }

    }

    private static boolean[] decodeBooleanArray(BooleanType.BooleanEncoding constructor, final int count)
    {
        boolean[] array = new boolean[count];

        for(int i = 0; i < count; i++)
        {
            array[i] = constructor.readPrimitiveValue();
        }

        return array;
    }

    private static byte[] decodeByteArray(ByteType.ByteEncoding constructor , final int count)
    {
        byte[] array = new byte[count];

        for(int i = 0; i < count; i++)
        {
            array[i] = constructor.readPrimitiveValue();
        }

        return array;
    }

    private static short[] decodeShortArray(ShortType.ShortEncoding constructor, final int count)
    {
        short[] array = new short[count];

        for(int i = 0; i < count; i++)
        {
            array[i] = constructor.readPrimitiveValue();
        }

        return array;
    }

    private static int[] decodeIntArray(IntegerType.IntegerEncoding constructor, final int count)
    {
        int[] array = new int[count];

        for(int i = 0; i < count; i++)
        {
            array[i] = constructor.readPrimitiveValue();
        }

        return array;
    }


    private static long[] decodeLongArray(LongType.LongEncoding constructor, final int count)
    {
        long[] array = new long[count];

        for(int i = 0; i < count; i++)
        {
            array[i] = constructor.readPrimitiveValue();
        }

        return array;
    }

    private static float[] decodeFloatArray(FloatType.FloatEncoding constructor, final int count)
    {
        float[] array = new float[count];

        for(int i = 0; i < count; i++)
        {
            array[i] = constructor.readPrimitiveValue();
        }

        return array;
    }

    private static double[] decodeDoubleArray(DoubleType.DoubleEncoding constructor, final int count)
    {
        double[] array = new double[count];

        for(int i = 0; i < count; i++)
        {
            array[i] = constructor.readPrimitiveValue();
        }

        return array;
    }




}

