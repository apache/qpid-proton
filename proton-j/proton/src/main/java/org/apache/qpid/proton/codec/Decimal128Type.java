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

import org.apache.qpid.proton.amqp.Decimal128;

import java.util.Collection;
import java.util.Collections;

public class Decimal128Type extends AbstractPrimitiveType<Decimal128>
{
    private Decimal128Encoding _decimal128Encoder;

    Decimal128Type(final EncoderImpl encoder, final DecoderImpl decoder)
    {
        _decimal128Encoder = new Decimal128Encoding(encoder, decoder);
        encoder.register(Decimal128.class, this);
        decoder.register(this);
    }

    public Class<Decimal128> getTypeClass()
    {
        return Decimal128.class;
    }

    public Decimal128Encoding getEncoding(final Decimal128 val)
    {
        return _decimal128Encoder;
    }


    public Decimal128Encoding getCanonicalEncoding()
    {
        return _decimal128Encoder;
    }

    public Collection<Decimal128Encoding> getAllEncodings()
    {
        return Collections.singleton(_decimal128Encoder);
    }

    private class Decimal128Encoding extends FixedSizePrimitiveTypeEncoding<Decimal128>
    {

        public Decimal128Encoding(final EncoderImpl encoder, final DecoderImpl decoder)
        {
            super(encoder, decoder);
        }

        @Override
        protected int getFixedSize()
        {
            return 16;
        }

        @Override
        public byte getEncodingCode()
        {
            return EncodingCodes.DECIMAL128;
        }

        public Decimal128Type getType()
        {
            return Decimal128Type.this;
        }

        public void writeValue(final Decimal128 val)
        {
            getEncoder().writeRaw(val.getMostSignificantBits());
            getEncoder().writeRaw(val.getLeastSignificantBits());
        }

        public boolean encodesSuperset(final TypeEncoding<Decimal128> encoding)
        {
            return (getType() == encoding.getType());
        }

        public Decimal128 readValue()
        {
            long msb = getDecoder().readRawLong();
            long lsb = getDecoder().readRawLong();
            return new Decimal128(msb, lsb);
        }
    }
}
