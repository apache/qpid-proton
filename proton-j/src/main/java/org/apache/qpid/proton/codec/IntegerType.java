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

import java.util.Arrays;
import java.util.Collection;

public class IntegerType extends AbstractPrimitiveType<Integer>
{

    public static interface IntegerEncoding extends PrimitiveTypeEncoding<Integer>
    {
        void write(WritableBuffer buffer, int i);
        void writeValue(WritableBuffer buffer, int i);
        int readPrimitiveValue(ReadableBuffer buffer);
    }

    private IntegerEncoding _integerEncoding;
    private IntegerEncoding _smallIntegerEncoding;

    IntegerType(final EncoderImpl encoder, final DecoderImpl decoder)
    {
        _integerEncoding = new AllIntegerEncoding(encoder, decoder);
        _smallIntegerEncoding = new SmallIntegerEncoding(encoder, decoder);
        encoder.register(Integer.class, this);
        decoder.register(this);
    }

    public Class<Integer> getTypeClass()
    {
        return Integer.class;
    }

    public IntegerEncoding getEncoding(final Integer val)
    {
        return getEncoding(val.intValue());
    }

    public IntegerEncoding getEncoding(final int i)
    {

        return (i >= -128 && i <= 127) ? _smallIntegerEncoding : _integerEncoding;
    }


    public IntegerEncoding getCanonicalEncoding()
    {
        return _integerEncoding;
    }

    public Collection<IntegerEncoding> getAllEncodings()
    {
        return Arrays.asList(_integerEncoding, _smallIntegerEncoding);
    }

    public void write(WritableBuffer buffer, int i)
    {
        if(i >= -128 && i <= 127)
        {
            _smallIntegerEncoding.write(buffer, i);
        }
        else
        {
            _integerEncoding.write(buffer, i);
        }
    }
    
    private class AllIntegerEncoding extends FixedSizePrimitiveTypeEncoding<Integer> implements IntegerEncoding
    {

        public AllIntegerEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
        {
            super(encoder, decoder);
        }

        @Override
        protected int getFixedSize()
        {
            return 4;
        }

        @Override
        public byte getEncodingCode()
        {
            return EncodingCodes.INT;
        }

        public IntegerType getType()
        {
            return IntegerType.this;
        }

        public void writeValue(WritableBuffer buffer, final Integer val)
        {
            getEncoder().writeRaw(buffer, val.intValue());
        }
        
        public void write(WritableBuffer buffer, final int i)
        {
            writeConstructor(buffer);
            getEncoder().writeRaw(buffer, i);
            
        }

        public void writeValue(WritableBuffer buffer, final int i)
        {
            getEncoder().writeRaw(buffer, i);
        }

        public int readPrimitiveValue(ReadableBuffer buffer)
        {
            return getDecoder().readRawInt(buffer);
        }

        public boolean encodesSuperset(final TypeEncoding<Integer> encoding)
        {
            return (getType() == encoding.getType());
        }

        public Integer readValue(ReadableBuffer buffer)
        {
            return readPrimitiveValue(buffer);
        }


        @Override
        public boolean encodesJavaPrimitive()
        {
            return true;
        }
    }

    private class SmallIntegerEncoding  extends FixedSizePrimitiveTypeEncoding<Integer> implements IntegerEncoding
    {
        public SmallIntegerEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
        {
            super(encoder, decoder);
        }

        @Override
        public byte getEncodingCode()
        {
            return EncodingCodes.SMALLINT;
        }

        @Override
        protected int getFixedSize()
        {
            return 1;
        }

        public void write(WritableBuffer buffer, final int i)
        {
            writeConstructor(buffer);
            getEncoder().writeRaw(buffer, (byte)i);
        }

        public void writeValue(WritableBuffer buffer, final int i)
        {
            getEncoder().writeRaw(buffer, (byte)i);
        }

        public int readPrimitiveValue(ReadableBuffer buffer)
        {
            return getDecoder().readRawByte(buffer);
        }

        public IntegerType getType()
        {
            return IntegerType.this;
        }

        public void writeValue(WritableBuffer buffer, final Integer val)
        {
            getEncoder().writeRaw(buffer, (byte)val.intValue());
        }

        public boolean encodesSuperset(final TypeEncoding<Integer> encoder)
        {
            return encoder == this;
        }

        public Integer readValue(ReadableBuffer buffer)
        {
            return readPrimitiveValue(buffer);
        }


        @Override
        public boolean encodesJavaPrimitive()
        {
            return true;
        }
    }
}
