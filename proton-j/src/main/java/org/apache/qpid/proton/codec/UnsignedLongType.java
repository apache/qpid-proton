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

import org.apache.qpid.proton.amqp.UnsignedLong;

import java.util.Arrays;
import java.util.Collection;

public class UnsignedLongType extends AbstractPrimitiveType<UnsignedLong>
{
    public static interface UnsignedLongEncoding extends PrimitiveTypeEncoding<UnsignedLong>
    {

    }

    private UnsignedLongEncoding _unsignedLongEncoding;
    private UnsignedLongEncoding _smallUnsignedLongEncoding;
    private UnsignedLongEncoding _zeroUnsignedLongEncoding;
    
    
    UnsignedLongType(final EncoderImpl encoder, final DecoderImpl decoder)
    {
        _unsignedLongEncoding = new AllUnsignedLongEncoding(encoder, decoder);
        _smallUnsignedLongEncoding = new SmallUnsignedLongEncoding(encoder, decoder);
        _zeroUnsignedLongEncoding = new ZeroUnsignedLongEncoding(encoder, decoder);
        encoder.register(UnsignedLong.class, this);
        decoder.register(this);
    }

    public Class<UnsignedLong> getTypeClass()
    {
        return UnsignedLong.class;
    }

    public UnsignedLongEncoding getEncoding(final UnsignedLong val)
    {
        long l = val.longValue();
        return l == 0L
            ? _zeroUnsignedLongEncoding
            : (l >= 0 && l <= 255L) ? _smallUnsignedLongEncoding : _unsignedLongEncoding;
    }


    public UnsignedLongEncoding getCanonicalEncoding()
    {
        return _unsignedLongEncoding;
    }

    public Collection<UnsignedLongEncoding> getAllEncodings()
    {
        return Arrays.asList(_zeroUnsignedLongEncoding, _smallUnsignedLongEncoding, _unsignedLongEncoding);
    }


    private class AllUnsignedLongEncoding
            extends FixedSizePrimitiveTypeEncoding<UnsignedLong>
            implements UnsignedLongEncoding
    {

        public AllUnsignedLongEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
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
            return EncodingCodes.ULONG;
        }

        public UnsignedLongType getType()
        {
            return UnsignedLongType.this;
        }

        public void writeValue(final UnsignedLong val)
        {
            getEncoder().writeRaw(val.longValue());
        }


        public boolean encodesSuperset(final TypeEncoding<UnsignedLong> encoding)
        {
            return (getType() == encoding.getType());
        }

        public UnsignedLong readValue()
        {
            return UnsignedLong.valueOf(getDecoder().readRawLong());
        }
    }

    private class SmallUnsignedLongEncoding
            extends FixedSizePrimitiveTypeEncoding<UnsignedLong>
            implements UnsignedLongEncoding
    {
        public SmallUnsignedLongEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
        {
            super(encoder, decoder);
        }

        @Override
        public byte getEncodingCode()
        {
            return EncodingCodes.SMALLULONG;
        }

        @Override
        protected int getFixedSize()
        {
            return 1;
        }


        public UnsignedLongType getType()
        {
            return UnsignedLongType.this;
        }

        public void writeValue(final UnsignedLong val)
        {
            getEncoder().writeRaw((byte)val.longValue());
        }

        public boolean encodesSuperset(final TypeEncoding<UnsignedLong> encoder)
        {
            return encoder == this  || encoder instanceof ZeroUnsignedLongEncoding;
        }

        public UnsignedLong readValue()
        {
            return UnsignedLong.valueOf(((long)getDecoder().readRawByte())&0xffl);
        }
    }
    
    
    private class ZeroUnsignedLongEncoding
            extends FixedSizePrimitiveTypeEncoding<UnsignedLong>
            implements UnsignedLongEncoding
    {
        public ZeroUnsignedLongEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
        {
            super(encoder, decoder);
        }

        @Override
        public byte getEncodingCode()
        {
            return EncodingCodes.ULONG0;
        }

        @Override
        protected int getFixedSize()
        {
            return 0;
        }


        public UnsignedLongType getType()
        {
            return UnsignedLongType.this;
        }

        public void writeValue(final UnsignedLong val)
        {
        }

        public boolean encodesSuperset(final TypeEncoding<UnsignedLong> encoder)
        {
            return encoder == this;
        }

        public UnsignedLong readValue()
        {
            return UnsignedLong.ZERO;
        }
    }
}
