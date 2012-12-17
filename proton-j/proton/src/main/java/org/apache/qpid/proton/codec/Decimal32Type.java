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

import org.apache.qpid.proton.amqp.Decimal32;

import java.util.Collection;
import java.util.Collections;

public class Decimal32Type extends AbstractPrimitiveType<Decimal32>
{
    private Decimal32Encoding _decimal32Encoder;

    Decimal32Type(final EncoderImpl encoder, final DecoderImpl decoder)
    {
        _decimal32Encoder = new Decimal32Encoding(encoder, decoder);
        encoder.register(Decimal32.class, this);
        decoder.register(this);
    }

    public Class<Decimal32> getTypeClass()
    {
        return Decimal32.class;
    }

    public Decimal32Encoding getEncoding(final Decimal32 val)
    {
        return _decimal32Encoder;
    }


    public Decimal32Encoding getCanonicalEncoding()
    {
        return _decimal32Encoder;
    }

    public Collection<Decimal32Encoding> getAllEncodings()
    {
        return Collections.singleton(_decimal32Encoder);
    }

    private class Decimal32Encoding extends FixedSizePrimitiveTypeEncoding<Decimal32>
    {

        public Decimal32Encoding(final EncoderImpl encoder, final DecoderImpl decoder)
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
            return EncodingCodes.DECIMAL32;
        }

        public Decimal32Type getType()
        {
            return Decimal32Type.this;
        }

        public void writeValue(final Decimal32 val)
        {
            getEncoder().writeRaw(val.getBits());
        }

        public boolean encodesSuperset(final TypeEncoding<Decimal32> encoding)
        {
            return (getType() == encoding.getType());
        }

        public Decimal32 readValue()
        {
            return new Decimal32(getDecoder().readRawInt());
        }
    }
}
