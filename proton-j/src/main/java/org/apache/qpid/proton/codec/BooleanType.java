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

public final class BooleanType extends AbstractPrimitiveType<Boolean>
{

    private static final byte BYTE_0 = (byte) 0;
    private static final byte BYTE_1 = (byte) 1;

    private org.apache.qpid.proton.codec.BooleanType.BooleanEncoding _trueEncoder;
    private org.apache.qpid.proton.codec.BooleanType.BooleanEncoding _falseEncoder;
    private org.apache.qpid.proton.codec.BooleanType.BooleanEncoding _booleanEncoder;

    public static interface BooleanEncoding extends PrimitiveTypeEncoding<Boolean>
    {
        void write(WritableBuffer buffer, boolean b);
        void writeValue(WritableBuffer buffer, boolean b);

        boolean readPrimitiveValue(ReadableBuffer buffer);
    }

    BooleanType(final EncoderImpl encoder, final DecoderImpl decoder)
    {
        _trueEncoder    = new TrueEncoding(encoder, decoder);
        _falseEncoder   = new FalseEncoding(encoder, decoder);
        _booleanEncoder = new AllBooleanEncoding(encoder, decoder);

        encoder.register(Boolean.class, this);
        decoder.register(this);
    }

    public Class<Boolean> getTypeClass()
    {
        return Boolean.class;
    }

    public BooleanEncoding getEncoding(final Boolean val)
    {
        return val ? _trueEncoder : _falseEncoder;
    }

    public BooleanEncoding getEncoding(final boolean val)
    {
        return val ? _trueEncoder : _falseEncoder;
    }

    public void writeValue(WritableBuffer buffer, final boolean val)
    {
        getEncoding(val).write(buffer, val);
    }




    public BooleanEncoding getCanonicalEncoding()
    {
        return _booleanEncoder;
    }

    public Collection<BooleanEncoding> getAllEncodings()
    {
        return Arrays.asList(_trueEncoder, _falseEncoder, _booleanEncoder);
    }

    private class TrueEncoding extends FixedSizePrimitiveTypeEncoding<Boolean> implements BooleanEncoding
    {

        public TrueEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
        {
            super(encoder, decoder);
        }

        @Override
        protected int getFixedSize()
        {
            return 0;
        }

        @Override
        public byte getEncodingCode()
        {
            return EncodingCodes.BOOLEAN_TRUE;
        }

        public BooleanType getType()
        {
            return BooleanType.this;
        }

        public void writeValue(WritableBuffer buffer, final Boolean val)
        {
        }

        public void write(WritableBuffer buffer, final boolean b)
        {
            writeConstructor(buffer);
        }

        public void writeValue(WritableBuffer buffer, final boolean b)
        {
        }

        public boolean encodesSuperset(final TypeEncoding<Boolean> encoding)
        {
            return encoding == this;
        }

        public Boolean readValue(ReadableBuffer buffer)
        {
            return Boolean.TRUE;
        }

        public boolean readPrimitiveValue(ReadableBuffer buffers)
        {
            return true;
        }

        @Override
        public boolean encodesJavaPrimitive()
        {
            return true;
        }
    }


    private class FalseEncoding extends FixedSizePrimitiveTypeEncoding<Boolean> implements org.apache.qpid.proton.codec.BooleanType.BooleanEncoding
    {

        public FalseEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
        {
            super(encoder, decoder);
        }

        @Override
        protected int getFixedSize()
        {
            return 0;
        }

        @Override
        public byte getEncodingCode()
        {
            return EncodingCodes.BOOLEAN_FALSE;
        }

        public BooleanType getType()
        {
            return BooleanType.this;
        }

        public void writeValue(WritableBuffer buffer, final Boolean val)
        {
        }

        public void write(WritableBuffer buffer, final boolean b)
        {
            writeConstructor(buffer);
        }

        public void writeValue(WritableBuffer buffer, final boolean b)
        {
        }

        public boolean readPrimitiveValue(ReadableBuffer buffer)
        {
            return false;
        }

        public boolean encodesSuperset(final TypeEncoding<Boolean> encoding)
        {
            return encoding == this;
        }

        public Boolean readValue(ReadableBuffer buffer)
        {
            return Boolean.FALSE;
        }


        @Override
        public boolean encodesJavaPrimitive()
        {
            return true;
        }
    }

    private class AllBooleanEncoding extends FixedSizePrimitiveTypeEncoding<Boolean> implements BooleanEncoding
    {

        public AllBooleanEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
        {
            super(encoder, decoder);
        }

        public BooleanType getType()
        {
            return BooleanType.this;
        }

        @Override
        protected int getFixedSize()
        {
            return 1;
        }

        @Override
        public byte getEncodingCode()
        {
            return EncodingCodes.BOOLEAN;
        }

        public void writeValue(WritableBuffer buffer, final Boolean val)
        {
            getEncoder().writeRaw(buffer, val ? BYTE_1 : BYTE_0);
        }

        public void write(WritableBuffer buffer, final boolean val)
        {
            writeConstructor(buffer);
            getEncoder().writeRaw(buffer, val ? BYTE_1 : BYTE_0);
        }

        public void writeValue(WritableBuffer buffer, final boolean b)
        {
            getEncoder().writeRaw(buffer, b ? BYTE_1 : BYTE_0);
        }

        public boolean readPrimitiveValue(ReadableBuffer buffer)
        {

            return getDecoder().readRawByte(buffer) != BYTE_0;
        }

        public boolean encodesSuperset(final TypeEncoding<Boolean> encoding)
        {
            return (getType() == encoding.getType());
        }

        public Boolean readValue(ReadableBuffer buffer)
        {
            return readPrimitiveValue(buffer) ? Boolean.TRUE : Boolean.FALSE;
        }


        @Override
        public boolean encodesJavaPrimitive()
        {
            return true;
        }
    }
}
