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

public class LongType extends AbstractPrimitiveType<Long>
{

    public static interface LongEncoding extends PrimitiveTypeEncoding<Long>
    {
        void write(WritableBuffer buffer, long l);
        void writeValue(WritableBuffer buffer, long l);
        public long readPrimitiveValue(ReadableBuffer buffer);
    }
    
    private LongEncoding _longEncoding;
    private LongEncoding _smallLongEncoding;

    LongType(final EncoderImpl encoder, final DecoderImpl decoder)
    {
        _longEncoding = new AllLongEncoding(encoder, decoder);
        _smallLongEncoding = new SmallLongEncoding(encoder, decoder);
        encoder.register(Long.class, this);
        decoder.register(this);
    }

    public Class<Long> getTypeClass()
    {
        return Long.class;
    }

    public LongEncoding getEncoding(final Long val)
    {
        return getEncoding(val.longValue());
    }

    public LongEncoding getEncoding(final long l)
    {
        return (l >= -128l && l <= 127l) ? _smallLongEncoding : _longEncoding;
    }


    public LongEncoding getCanonicalEncoding()
    {
        return _longEncoding;
    }

    public Collection<LongEncoding> getAllEncodings()
    {
        return Arrays.asList(_smallLongEncoding, _longEncoding);
    }

    public void write(WritableBuffer buffer, long l)
    {
        if(l >= -128l && l <= 127l)
        {
            _smallLongEncoding.write(buffer, l);
        }
        else
        {
            _longEncoding.write(buffer, l);
        }
    }
    
    private class AllLongEncoding extends FixedSizePrimitiveTypeEncoding<Long> implements LongEncoding
    {

        public AllLongEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
        {
            super(encoder, decoder);
        }

        @Override
        protected int getFixedSize()
        {
            return 8;
        }

        @Override
        public byte getEncodingCode()
        {
            return EncodingCodes.LONG;
        }

        public LongType getType()
        {
            return LongType.this;
        }

        public void writeValue(WritableBuffer buffer, final Long val)
        {
            getEncoder().writeRaw(buffer, val.longValue());
        }
        
        public void write(WritableBuffer buffer, final long l)
        {
            writeConstructor(buffer);
            getEncoder().writeRaw(buffer, l);
            
        }

        public void writeValue(WritableBuffer buffer, final long l)
        {
            getEncoder().writeRaw(buffer, l);
        }

        public boolean encodesSuperset(final TypeEncoding<Long> encoding)
        {
            return (getType() == encoding.getType());
        }

        public Long readValue(ReadableBuffer buffer)
        {
            return readPrimitiveValue(buffer);
        }

        public long readPrimitiveValue(ReadableBuffer buffer)
        {
            return getDecoder().readRawLong(buffer);
        }


        @Override
        public boolean encodesJavaPrimitive()
        {
            return true;
        }
    }

    private class SmallLongEncoding  extends FixedSizePrimitiveTypeEncoding<Long> implements LongEncoding
    {
        public SmallLongEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
        {
            super(encoder, decoder);
        }

        @Override
        public byte getEncodingCode()
        {
            return EncodingCodes.SMALLLONG;
        }

        @Override
        protected int getFixedSize()
        {
            return 1;
        }

        public void write(WritableBuffer buffer, final long l)
        {
            writeConstructor(buffer);
            getEncoder().writeRaw(buffer, (byte)l);
        }

        public void writeValue(WritableBuffer buffer, final long l)
        {
            getEncoder().writeRaw(buffer, (byte)l);
        }

        public long readPrimitiveValue(ReadableBuffer buffer)
        {
            return (long) getDecoder().readRawByte(buffer);
        }

        public LongType getType()
        {
            return LongType.this;
        }

        public void writeValue(WritableBuffer buffer, final Long val)
        {
            getEncoder().writeRaw(buffer, (byte)val.longValue());
        }

        public boolean encodesSuperset(final TypeEncoding<Long> encoder)
        {
            return encoder == this;
        }

        public Long readValue(ReadableBuffer buffer)
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
