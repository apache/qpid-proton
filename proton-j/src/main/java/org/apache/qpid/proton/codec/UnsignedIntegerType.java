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

import org.apache.qpid.proton.amqp.UnsignedInteger;

import java.util.Arrays;
import java.util.Collection;

public class UnsignedIntegerType extends AbstractPrimitiveType<UnsignedInteger>
{
    public static interface UnsignedIntegerEncoding extends PrimitiveTypeEncoding<UnsignedInteger>
    {

    }

    private UnsignedIntegerEncoding _unsignedIntegerEncoding;
    private UnsignedIntegerEncoding _smallUnsignedIntegerEncoding;
    private UnsignedIntegerEncoding _zeroUnsignedIntegerEncoding;


    UnsignedIntegerType(final EncoderImpl encoder, final DecoderImpl decoder)
    {
        _unsignedIntegerEncoding = new AllUnsignedIntegerEncoding(encoder, decoder);
        _smallUnsignedIntegerEncoding = new SmallUnsignedIntegerEncoding(encoder, decoder);
        _zeroUnsignedIntegerEncoding = new ZeroUnsignedIntegerEncoding(encoder, decoder);
        encoder.register(UnsignedInteger.class, this);
        decoder.register(this);
    }

    public Class<UnsignedInteger> getTypeClass()
    {
        return UnsignedInteger.class;
    }

    public UnsignedIntegerEncoding getEncoding(final UnsignedInteger val)
    {
        int i = val.intValue();
        return i == 0
            ? _zeroUnsignedIntegerEncoding
            : (i >= 0 && i <= 255) ? _smallUnsignedIntegerEncoding : _unsignedIntegerEncoding;
    }


    public UnsignedIntegerEncoding getCanonicalEncoding()
    {
        return _unsignedIntegerEncoding;
    }

    public Collection<UnsignedIntegerEncoding> getAllEncodings()
    {
        return Arrays.asList(_unsignedIntegerEncoding, _smallUnsignedIntegerEncoding, _zeroUnsignedIntegerEncoding);
    }


    private class AllUnsignedIntegerEncoding
            extends FixedSizePrimitiveTypeEncoding<UnsignedInteger>
            implements UnsignedIntegerEncoding
    {

        public AllUnsignedIntegerEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
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
            return EncodingCodes.UINT;
        }

        public UnsignedIntegerType getType()
        {
            return UnsignedIntegerType.this;
        }

        public void writeValue(final UnsignedInteger val)
        {
            getEncoder().writeRaw(val.intValue());
        }
        
        public void write(final int i)
        {
            writeConstructor();
            getEncoder().writeRaw(i);
            
        }

        public boolean encodesSuperset(final TypeEncoding<UnsignedInteger> encoding)
        {
            return (getType() == encoding.getType());
        }

        public UnsignedInteger readValue()
        {
            return UnsignedInteger.valueOf(getDecoder().readRawInt());
        }
    }

    private class SmallUnsignedIntegerEncoding
            extends FixedSizePrimitiveTypeEncoding<UnsignedInteger>
            implements UnsignedIntegerEncoding
    {
        public SmallUnsignedIntegerEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
        {
            super(encoder, decoder);
        }

        @Override
        public byte getEncodingCode()
        {
            return EncodingCodes.SMALLUINT;
        }

        @Override
        protected int getFixedSize()
        {
            return 1;
        }


        public UnsignedIntegerType getType()
        {
            return UnsignedIntegerType.this;
        }

        public void writeValue(final UnsignedInteger val)
        {
            getEncoder().writeRaw((byte)val.intValue());
        }

        public boolean encodesSuperset(final TypeEncoding<UnsignedInteger> encoder)
        {
            return encoder == this  || encoder instanceof ZeroUnsignedIntegerEncoding;
        }

        public UnsignedInteger readValue()
        {
            return UnsignedInteger.valueOf(((int)getDecoder().readRawByte()) & 0xff);
        }
    }


    private class ZeroUnsignedIntegerEncoding
            extends FixedSizePrimitiveTypeEncoding<UnsignedInteger>
            implements UnsignedIntegerEncoding
    {
        public ZeroUnsignedIntegerEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
        {
            super(encoder, decoder);
        }

        @Override
        public byte getEncodingCode()
        {
            return EncodingCodes.UINT0;
        }

        @Override
        protected int getFixedSize()
        {
            return 0;
        }


        public UnsignedIntegerType getType()
        {
            return UnsignedIntegerType.this;
        }

        public void writeValue(final UnsignedInteger val)
        {
        }

        public boolean encodesSuperset(final TypeEncoding<UnsignedInteger> encoder)
        {
            return encoder == this;
        }

        public UnsignedInteger readValue()
        {
            return UnsignedInteger.ZERO;
        }
    }
}
