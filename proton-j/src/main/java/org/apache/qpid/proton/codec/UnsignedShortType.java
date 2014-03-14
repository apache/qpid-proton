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

import org.apache.qpid.proton.amqp.UnsignedShort;

import java.util.Collection;
import java.util.Collections;

public class UnsignedShortType extends AbstractPrimitiveType<UnsignedShort>
{
    private UnsignedShortEncoding _unsignedShortEncoder;

    UnsignedShortType(final EncoderImpl encoder, final DecoderImpl decoder)
    {
        _unsignedShortEncoder = new UnsignedShortEncoding(encoder, decoder);
        encoder.register(UnsignedShort.class, this);
        decoder.register(this);
    }

    public Class<UnsignedShort> getTypeClass()
    {
        return UnsignedShort.class;
    }

    public UnsignedShortEncoding getEncoding(final UnsignedShort val)
    {
        return _unsignedShortEncoder;
    }


    public UnsignedShortEncoding getCanonicalEncoding()
    {
        return _unsignedShortEncoder;
    }

    public Collection<UnsignedShortEncoding> getAllEncodings()
    {
        return Collections.singleton(_unsignedShortEncoder);
    }

    private class UnsignedShortEncoding extends FixedSizePrimitiveTypeEncoding<UnsignedShort>
    {

        public UnsignedShortEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
        {
            super(encoder, decoder);
        }

        @Override
        protected int getFixedSize()
        {
            return 2;
        }

        @Override
        public byte getEncodingCode()
        {
            return EncodingCodes.USHORT;
        }

        public UnsignedShortType getType()
        {
            return UnsignedShortType.this;
        }

        public void writeValue(final UnsignedShort val)
        {
            getEncoder().writeRaw(val.shortValue());
        }

        public boolean encodesSuperset(final TypeEncoding<UnsignedShort> encoding)
        {
            return (getType() == encoding.getType());
        }

        public UnsignedShort readValue()
        {
            return UnsignedShort.valueOf(getDecoder().readRawShort());
        }
    }
}
