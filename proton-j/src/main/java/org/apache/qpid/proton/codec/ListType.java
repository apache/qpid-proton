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

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.List;

public class ListType extends AbstractPrimitiveType<List>
{
    private final ListEncoding _listEncoding;
    private final ListEncoding _shortListEncoding;
    private final ListEncoding _zeroListEncoding;
    private EncoderImpl _encoder;

    public static interface ListEncoding extends PrimitiveTypeEncoding<List>
    {
        Object readElement(ReadableBuffer buffer, DecoderImpl decoder);
        void setValue(List value, int length);
        int readCount(ReadableBuffer buffer, DecoderImpl decoder);
    }

    ListType(final EncoderImpl encoder, final DecoderImpl decoder)
    {
        _encoder = encoder;
        _listEncoding = new AllListEncoding(encoder, decoder);
        _shortListEncoding = new ShortListEncoding(encoder, decoder);
        _zeroListEncoding = new ZeroListEncoding(encoder, decoder);
        encoder.register(List.class, this);
        decoder.register(this);
    }

    public Class<List> getTypeClass()
    {
        return List.class;
    }

    public ListEncoding getEncoding(final List val)
    {

        int calculatedSize = calculateSize(val, _encoder);
        ListEncoding encoding = val.isEmpty() 
                                    ? _zeroListEncoding 
                                    : (val.size() > 255 || calculatedSize >= 254)
                                        ? _listEncoding
                                        : _shortListEncoding;

        encoding.setValue(val, calculatedSize);
        return encoding;
    }

    private static int calculateSize(final List val, EncoderImpl encoder)
    {
        int len = 0;
        final int count = val.size();

        for(int i = 0; i < count; i++)
        {
            Object element = val.get(i);
            AMQPType type = encoder.getType(element);
            if(type == null)
            {
                throw new IllegalArgumentException("No encoding defined for type: " + element.getClass());
            }
            TypeEncoding elementEncoding = type.getEncoding(element);
            len += elementEncoding.getConstructorSize()+elementEncoding.getValueSize(element);
        }
        return len;
    }


    public ListEncoding getCanonicalEncoding()
    {
        return _listEncoding;
    }

    public Collection<ListEncoding> getAllEncodings()
    {
        return Arrays.asList(_zeroListEncoding, _shortListEncoding, _listEncoding);
    }

    private class AllListEncoding
            extends LargeFloatingSizePrimitiveTypeEncoding<List>
            implements ListEncoding
    {

        public AllListEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
        {
            super(encoder, decoder);
        }

        @Override
        protected void writeEncodedValue(WritableBuffer buffer, final List val)
        {
            getEncoder().writeRaw(buffer, val.size());

            final int count = val.size();

            for(int i = 0; i < count; i++)
            {
                Object element = val.get(i);
                TypeEncoding elementEncoding = getEncoder().getType(element).getEncoding(element);
                elementEncoding.writeConstructor(buffer);
                elementEncoding.writeValue(buffer, element);
            }
        }

        @Override
        protected int getEncodedValueSize(final List val)
        {
            CachedCalculation cachedCalculation = CachedCalculation.getCache();
            return 4 + ((val == cachedCalculation.getVal()) ? cachedCalculation.getSize() : calculateSize(val, getEncoder()));
        }


        @Override
        public byte getEncodingCode()
        {
            return EncodingCodes.LIST32;
        }

        public ListType getType()
        {
            return ListType.this;
        }

        public boolean encodesSuperset(final TypeEncoding<List> encoding)
        {
            return (getType() == encoding.getType());
        }

        public List readValue(ReadableBuffer buffer)
        {
            DecoderImpl decoder = getDecoder();
            int size = decoder.readRawInt(buffer);
            // todo - limit the decoder with size
            int count = decoder.readRawInt(buffer);
            // Ensure we do not allocate an array of size greater then the available data, otherwise there is a risk for an OOM error
            if (count > decoder.getByteBufferRemaining(buffer)) {
                throw new IllegalArgumentException("List element count "+count+" is specified to be greater than the amount of data available ("+
                                                   decoder.getByteBufferRemaining(buffer)+")");
            }
            List list = new ArrayList(count);
            for(int i = 0; i < count; i++)
            {
                list.add(readElement(buffer, decoder));
            }
            return list;
        }

        public void setValue(final List value, final int length)
        {
            CachedCalculation.setCachedValue(value, length);
        }

        public Object readElement(ReadableBuffer buffer, DecoderImpl decoder)
        {
            return decoder.readObject(buffer);
        }


        public int readCount(ReadableBuffer buffer, DecoderImpl decoder)
        {
            int size = decoder.readRawInt(buffer);
            // todo - limit the decoder with size
            return decoder.readRawInt(buffer);

        }
    }

    private class ShortListEncoding
            extends SmallFloatingSizePrimitiveTypeEncoding<List>
            implements ListEncoding
    {

        public ShortListEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
        {
            super(encoder, decoder);
        }

        @Override
        protected void writeEncodedValue(WritableBuffer buffer, final List val)
        {
            getEncoder().writeRaw(buffer, (byte)val.size());

            final int count = val.size();

            for(int i = 0; i < count; i++)
            {
                Object element = val.get(i);
                TypeEncoding elementEncoding = getEncoder().getType(element).getEncoding(element);
                elementEncoding.writeConstructor(buffer);
                elementEncoding.writeValue(buffer, element);
            }
        }

        @Override
        protected int getEncodedValueSize(final List val)
        {
            CachedCalculation cachedCalculation = CachedCalculation.getCache();
            return 1 + ((val == cachedCalculation.getVal()) ? cachedCalculation.getSize() : calculateSize(val, getEncoder()));
        }


        @Override
        public byte getEncodingCode()
        {
            return EncodingCodes.LIST8;
        }

        public ListType getType()
        {
            return ListType.this;
        }

        public boolean encodesSuperset(final TypeEncoding<List> encoder)
        {
            return encoder == this;
        }


        public void setValue(final List value, final int length)
        {
            CachedCalculation.setCachedValue(value, length);
        }


        public List readValue(ReadableBuffer buffer)
        {

            DecoderImpl decoder = getDecoder();
            int count = readCount(buffer, decoder);
            List list = new ArrayList(count);
            for(int i = 0; i < count; i++)
            {
                list.add(readElement(buffer, decoder));
            }
            return list;
        }

        public Object readElement(ReadableBuffer buffer, DecoderImpl decoder)
        {
            return decoder.readObject(buffer);
        }

        public int readCount(ReadableBuffer buffer, DecoderImpl decoder)
        {
            int size = ((int)decoder.readRawByte(buffer)) & 0xff;
            // todo - limit the decoder with size
            return ((int)decoder.readRawByte(buffer)) & 0xff;
        }

    }

    
    private class ZeroListEncoding
            extends FixedSizePrimitiveTypeEncoding<List>
            implements ListEncoding
    {
        public ZeroListEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
        {
            super(encoder, decoder);
        }

        @Override
        public byte getEncodingCode()
        {
            return EncodingCodes.LIST0;
        }

        @Override
        protected int getFixedSize()
        {
            return 0;
        }


        public ListType getType()
        {
           return ListType.this;
        }

        public void writeValue(WritableBuffer buffer, final List val)
        {
        }

        public Object readElement(ReadableBuffer buffer, DecoderImpl decoder)
        {
            return null;
        }


        public int readCount(ReadableBuffer buffer, DecoderImpl decoder)
        {
            return 0;
        }


        public void setValue(final List value, final int length)
        {
        }

        public boolean encodesSuperset(final TypeEncoding<List> encoder)
        {
            return encoder == this;
        }

        public List readValue(ReadableBuffer buffer)
        {
            return Collections.EMPTY_LIST;
        }


    }
}
