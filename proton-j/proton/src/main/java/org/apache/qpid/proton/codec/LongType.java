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
        void write(long l);
        void writeValue(long l);
        public long readPrimitiveValue();
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

    public void write(long l)
    {
        if(l >= -128l && l <= 127l)
        {
            _smallLongEncoding.write(l);
        }
        else
        {
            _longEncoding.write(l);
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

        public void writeValue(final Long val)
        {
            getEncoder().writeRaw(val.longValue());
        }
        
        public void write(final long l)
        {
            writeConstructor();
            getEncoder().writeRaw(l);
            
        }

        public void writeValue(final long l)
        {
            getEncoder().writeRaw(l);
        }

        public boolean encodesSuperset(final TypeEncoding<Long> encoding)
        {
            return (getType() == encoding.getType());
        }

        public Long readValue()
        {
            return readPrimitiveValue();
        }

        public long readPrimitiveValue()
        {
            return getDecoder().readRawLong();
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

        public void write(final long l)
        {
            writeConstructor();
            getEncoder().writeRaw((byte)l);
        }

        public void writeValue(final long l)
        {
            getEncoder().writeRaw((byte)l);
        }

        public long readPrimitiveValue()
        {
            return (long) getDecoder().readRawByte();
        }

        public LongType getType()
        {
            return LongType.this;
        }

        public void writeValue(final Long val)
        {
            getEncoder().writeRaw((byte)val.longValue());
        }

        public boolean encodesSuperset(final TypeEncoding<Long> encoder)
        {
            return encoder == this;
        }

        public Long readValue()
        {
            return readPrimitiveValue();
        }


        @Override
        public boolean encodesJavaPrimitive()
        {
            return true;
        }
    }
}
